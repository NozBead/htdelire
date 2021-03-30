#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <linux/limits.h>
#include "http_parse.h"
#include "stats.h"

#define BBUFFER_SIZE 256
#define BUFFER_SIZE 64

static char * doc_root;

int is_empty_line(const char * line){
	return strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0;
}

int is_below_docroot(char * path){
	int position = 0;
	char * delim = "/";

	char * chunk = strtok(path, delim);

	while(chunk != NULL){
		if(strcmp(chunk, "..") == 0){
			position--;
		} 
		else{
			position++;
		}

		if(position < 0)
			return 1;

		chunk = strtok(NULL, delim);
	}

	return 0;
}

int set_doc_root(char * path){
	struct stat st;

	if(stat(path, &st) == -1){
		perror(path);
		return -1;
	}

	if((st.st_mode & S_IFMT) == S_IFDIR){
		doc_root = path;
		return 0;
	}
	else{
		fprintf(stderr, "%s is not a directory\n" , path);
		return -1;
	}
}

char * fgets_or_exit(char * buffer, int size, FILE * client_stream){
	if(fgets(buffer, size, client_stream) == NULL){
		perror("Error reading one line of client stream");
		exit(-1);
	}

	return buffer;
}

int copy(FILE * in, FILE * out){
	unsigned char * buf[BBUFFER_SIZE];

	int bytes_read = fread(buf, 1, BBUFFER_SIZE,in);
	while(bytes_read > 0){
		if(fwrite(buf, 1, bytes_read, out) <= 0){
			fprintf(stderr, "Error while returning file content");
			return -1;
		}

		bytes_read = fread(buf, 1, BBUFFER_SIZE,in);
	}

	return 0;
}

void skip_headers(FILE * client_stream){
	char buffer[BBUFFER_SIZE];

	char * line = fgets_or_exit(	buffer,
					BBUFFER_SIZE,
					client_stream);

	while(!is_empty_line(line))
		line = fgets_or_exit(	buffer,
					BBUFFER_SIZE,
					client_stream);
}

char * get_status(int code, const char * reason){
	char * status = malloc(BUFFER_SIZE);
	sprintf(status, "HTTP/1.1 %d %s\r\n", code, reason);
	return status;
}

char * get_content_length(int size){
	char * content_length = malloc(BUFFER_SIZE);
	sprintf(content_length, "Content-Length: %d\r\n", size);
	return content_length;
}

char * get_file_mime(const char * path){
	int fd[2];

	if(pipe(fd) == -1){
		perror("Pipe");
	}

	int pid = fork();

	if(pid == -1){
		perror("Error creating child process to get the file mime");
	}
	else if(pid == 0){
		close(1);

		if(dup(fd[1]) == -1){
			perror("Error dupping standart input");
		}

		if(execlp("mimetype", "mimetype","-b", path, NULL) == -1){
			perror("Error executing file command");
		}
	}

	close(fd[1]);

	char * mime = malloc(BUFFER_SIZE);

	int bytes_read = read(fd[0], mime, BUFFER_SIZE);

	close(fd[0]);

	if(bytes_read == -1){
		perror("Error reading end of pipe");
	}

	mime[bytes_read - 1] = '\0';

	return mime;
}

char * get_content_type(const char * mime){
	char * content_type = malloc(BUFFER_SIZE);
	sprintf(content_type, "Content-Type: %s\r\n", mime);
	return content_type;
}

int get_file_size(int fd){
	struct stat st;

	if(fstat(fd, &st) == -1){
		perror("Error getting stat of file");
		return -1;
	}

	return st.st_size;
}

char * get_str_stats(web_stats * stats){
	char * text = malloc(BBUFFER_SIZE);

	sprintf(text, "Connections : %d\nRequêtes répondues : %d\n200 : %d\n400 : %d\n403 : %d\n404 : %d\n",
			stats->served_connections, stats->served_requests, stats->ok_200, stats->ko_400, stats->ko_403, stats->ko_404);

	return text;
}

int send_header_response(FILE * client_stream, int code, const char * reason, int content_length, const char * content_type){
	char * status = get_status(code, reason);
	char * cont_length = get_content_length(content_length);
	char * cont_type = get_content_type(content_type);
	
	int res = (fprintf(client_stream, "%s%s%s\r\n", status, cont_length, cont_type) <= 0) ? -1 : 0;

	free(status);
	free(cont_length);
	free(cont_type);

	return res;
}

int send_file(FILE * client_stream, FILE * target, const char * path){
	char * mime = get_file_mime(path);
	
	if(send_header_response(client_stream, 200, "OK",
						get_file_size(fileno(target)),mime) == -1)
		return -1;

	int res = copy(target, client_stream);

	free(mime);
	fclose(target);

	return res;
}

int send_text(FILE * client_stream, int code, const char * reason, const char * body){
	if(send_header_response(client_stream, code, reason,
						strlen(body),
						"text/plain ; charset=utf-8") == -1)
		return -1;
	else
		return (fprintf(client_stream, "%s", body) <= 0) ? -1 : 0;
}

int send_stats(FILE * client_stream){
	char * text = get_str_stats(get_stats());

	int res = send_text(client_stream, 200, "OK", text);

	free(text);

	return res;
}

char * get_relative_link(const char * doc_root, const char * filename){
	char * path = malloc(PATH_MAX);
	path[0] = '\0';

	strcat(path, doc_root);
  	strcat(path, filename);

	if(filename[strlen(filename) - 1] == '/'){
		strcat(path, "index.html");
	}

	return path;
}

FILE * check_and_open(const char * path){
	FILE * f;

	if((f = fopen(path, "r")) == NULL){
		perror(path);
		return NULL;
	}

	return f;
}

char * rewrite_target(char * target){
	char * request = target;
	int idx = 0;

	while(request[idx] != '\0' && request[idx] != '?'){
		idx++;
	}

	request[idx] = '\0';

	return request;
}

void http_treatment(FILE * client_stream){
	int bad_request = 0;
	int target_found = 0;
	int is_stat = 0;

	char buffer[BUFFER_SIZE];

	http_request request;
	char * line = fgets_or_exit(	buffer,
					BUFFER_SIZE,
					client_stream);

	if(parse_http_request(line, &request) == 0){
		bad_request = 1;
	}

	skip_headers(client_stream);

	char * raw_target = rewrite_target(request.target);
	char * path;
	FILE * target;

	if(strcmp(raw_target,"/stats") != 0){
		path = get_relative_link(doc_root, raw_target);
		target = check_and_open(path);

	  	target_found = target != NULL;
	}
	else{
		is_stat = 1;
	}

	incr_stat(&get_stats()->served_requests);

	if(bad_request){
		if(send_text(client_stream, 400, "Bad Request", "Bad Request")  != -1)
			incr_stat(&get_stats()->ko_400);
	}
	else if(is_stat){
		if(send_stats(client_stream) != -1)
			incr_stat(&get_stats()->ok_200);
	}
	else if(is_below_docroot(raw_target)){
		if(send_text(client_stream, 403, "Forbidden", "Forbidden") != -1)
			incr_stat(&get_stats()->ko_403);
	}
	else if(target_found){
	  	if(send_file(client_stream, target, path) != -1)
			incr_stat(&get_stats()->ok_200);
	}
	else{
		if(send_text(client_stream, 404, "Not Found", "Not Found") != -1)
			incr_stat(&get_stats()->ko_404);
	}

	if(!is_stat)
		free(path);
}
