#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

typedef void handler_t (int);

void kill_zombie(int sig){
	int status = sig;
	while(waitpid(-1, &status, WNOHANG) > 0);
}

void no_action(int sig){
	printf("SIGPIPE Detected : %d\n", sig);
}

int setup_handler(int sig, handler_t fct){
	struct sigaction action;
	action.sa_handler = fct;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_RESTART;

	if(sigaction(sig, &action, NULL) == -1){
		perror("Error attaching action to signal");
		return -1;
	}

	return 0;
}


int initialize_signals(void){
	return (setup_handler(SIGPIPE, no_action) != -1 && setup_handler(SIGCHLD, kill_zombie) != -1) ? 0 : -1;
}
