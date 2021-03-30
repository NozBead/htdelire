#include <stdlib.h>
#include <stdio.h>
#include "http.h"
#include "socket.h"
#include "daycare.h"

int main(int argc, char ** argv){
	int port = 8080;

	if(argc == 3)
		port = atoi(argv[2]);
	else if(argc < 2)
		return 2;

	int server = create_server(port);

	if(server == -1)
		return 3;

	if(initialize_signals() == -1)
		return 4;

	if(set_doc_root(argv[1]) == -1)
		return 5;

	return (run_server(server, http_treatment) == -1) ? 6 : 0;
}
