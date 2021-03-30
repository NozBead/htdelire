#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "socket.h"
#include "stats.h"

int create_server(int port){
	int server_socket;
	if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("Error creating socket");
		return -1;
	}

	int valueoptions = 1;
	if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &valueoptions, sizeof(int)) == -1){
		perror("Error enabling server socket options");
		return -1;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1){
		perror("Error socket binding");
		return -1;
	}

	if(listen(server_socket, 20) == -1){
		perror("Error enabling listen");
		return -1;
	}

	return server_socket;
}

void display_connection(struct sockaddr_in addr){
	uint32_t ip_val = addr.sin_addr.s_addr;

	printf("Connection from %d.%d.%d.%d\n",ip_val & 0xFF,
						(ip_val >> 8) & 0xFF,
						(ip_val >> 16) & 0xFF,
						(ip_val >> 24) & 0xFF);
}

void client_treatment(int client_socket, client_handler handler){
	FILE * client_stream = fdopen(client_socket, "a+");

	handler(client_stream);

	fclose(client_stream);
}

int run_server(int server_socket, client_handler handler){
	init_stats();

	while(1){
		int client_socket;
		struct sockaddr_in client_addr;
		socklen_t size = 32;

		if((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &size)) == -1){
			perror("Error client connection");
			return -1;
		}

		incr_stat(&get_stats()->served_connections);

		//display_connection(client_addr);

		int thread = fork();

		if(thread == 0){
			client_treatment(client_socket, handler);
			exit(7);
		}
		else{
			close(client_socket);
		}
	}

	return 0;
}
