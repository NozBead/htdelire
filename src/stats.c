#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include "stats.h"

static web_stats * stats;
static sem_t * sema;

int init_stats(void){
	void * area = mmap(NULL, sizeof(web_stats) + sizeof(sem_t), PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if(area == MAP_FAILED){
		perror("Error mapping shared memory");
		return -1;
	}

	stats = area;
	sema = area+sizeof(web_stats);

	if(sem_init(sema, 1, 1) == -1){
		perror("Error starting semaphore");
		return -1;
	}

	return 0;
}

web_stats * get_stats(void){
	return stats;
}

void incr_stat(int * webstat_field){
	if(sem_wait(sema) == -1)
		perror("Error acessing semaphore");

	(*webstat_field)++;

	if(sem_post(sema) == -1)
		perror("Error acessing semaphore");
}
