#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "list.h"
#include "threads/synch.h"
typedef int pid_t;
struct lock filesys_lock;
struct file_object{
    struct file *f;
    int fd;
    int is_open;
};

void syscall_init (void);
void check_ptr(const void *ptr);
void check_buffer (void *buff_to_check, unsigned size);
void exit_process (int status);
pid_t exec (const char *cmd_line);
int write (int fd, const void *buffer, unsigned size);
int open (const char *file);
void close (int fd);
int read (int fd, void *buffer, unsigned size);
int filesize (int fd);
#endif /* userprog/syscall.h */
