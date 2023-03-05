#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <list.h>
#include <stdbool.h>
struct dead_process{
    int tid;
    int status;
    bool waited_before;
    struct list_elem elem;
};

struct waiting_process{
    int tid;
    struct list_elem elem;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
