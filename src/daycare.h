#ifndef __DAYCARE__H
#define __DAYCARE__H

typedef void handler_t (int);

int setup_handler(int sig, handler_t fct);
int initialize_signals(void);

#endif
