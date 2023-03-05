#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "lib/kernel/stdio.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);


void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  check_ptr(f->esp);
  int sys_code = *(int*)f->esp;
  //printf("syscall code %d\n", sys_code);
  switch (sys_code){
    case SYS_HALT:
      shutdown_power_off();
      break;
    case SYS_EXIT:
      check_ptr((int*)f->esp + 1);
      int status = *((int*)f->esp + 1);
      exit_process(status);
      break;
    case SYS_EXEC:
      check_ptr((const char*)(*((int*)f->esp + 1)));
      const char* cmd = (const char*)(*((int*)f->esp + 1));
      pid_t id = exec(cmd);
      f->eax = id;
      break;
    case SYS_WAIT:
      check_ptr(f->esp + 1);
      pid_t pid = *((pid_t*)f->esp + 1);
      f->eax = process_wait((tid_t)pid);
      break;
    case SYS_CREATE:
      check_ptr((const char*)(*((int*)f->esp + 1)));
      check_ptr((unsigned*)f->esp + 2);
      const char* name = (const char*)(*((int*)f->esp + 1));
      unsigned s = *((unsigned*)f->esp + 2);
      lock_acquire(&filesys_lock);
      f->eax = filesys_create(name,(off_t)s);
      lock_release(&filesys_lock);
      break;
    case SYS_REMOVE:
      check_ptr((const char*)(*((int*)f->esp + 1)));
      lock_acquire(&filesys_lock);
      f->eax = filesys_remove((const char*)(*((int*)f->esp + 1)));
      lock_release(&filesys_lock);
      break;
    case SYS_OPEN:
      check_ptr((const char*)(*((int*)f->esp + 1)));
      int file_d = open((const char*)(*((int*)f->esp + 1)));
      //printf("file_d = %d\n", file_d);
      f->eax = file_d;
      break;
    case SYS_FILESIZE:
      check_ptr((int*)f->esp + 1);
      f->eax = filesize(*((int*)f->esp + 1));
      break;
    case SYS_READ:
      check_ptr((int*)f->esp + 1);
      check_ptr((void*)(*((int*)f->esp + 2)));
      check_ptr((unsigned*)f->esp + 3);
      int fd = *((int*)f->esp + 1);
      void* buffer = (void*)(*((int*)f->esp + 2));
      unsigned size = *((unsigned*)f->esp + 3);
      check_buffer(buffer, size);
      f->eax = read(fd, buffer, size);
      if ((int)f->eax == -1){
        exit_process(-1);
      }
      break;
    case SYS_WRITE:
      check_ptr(f->esp + 1);
      check_ptr(f->esp + 2);
      check_ptr(f->esp + 3);
      int fd2 = *((int*)f->esp + 1);
      void* buff = (void*)(*((int*)f->esp + 2));
      check_buffer(buff, fd2);
      unsigned size1 = *((unsigned*)f->esp + 3);
      f->eax = write(fd2,buff,size1);
      break;
    case SYS_SEEK:
      check_ptr((int*)f->esp + 1);
      check_ptr((unsigned*)f->esp + 2);
      int fd3 = *((int*)f->esp + 1);
      unsigned sizee = *((unsigned*)f->esp + 2);
      file_seek(thread_current()->fdt[fd3]->f,(off_t)sizee);
      break;
    case SYS_TELL:
      check_ptr((int*)f->esp + 1);
      int fd4 = *((int*)f->esp + 1);
      f->eax = (unsigned)file_tell(thread_current()->fdt[fd4]->f);
      break;
    case SYS_CLOSE:
      check_ptr((int*)f->esp + 1);
      int fd1 = *((int*)f->esp + 1);
      close(fd1);
      break;
    default:
      exit_process(0);
      break;
  }

}


pid_t exec (const char *cmd_line) {
  tid_t id = process_execute(cmd_line);
  return (pid_t) id;
}

int write (int fd, const void *buffer, unsigned size) {
  if (fd == 1){
    putbuf((const char*)buffer, size);
    return size;
  }else{
    int x = (int)file_write(thread_current()->fdt[fd]->f,buffer,(off_t)size);
    return x;
  }
}

void check_ptr(const void *ptr){
  if (!is_user_vaddr(ptr) || pagedir_get_page(
    thread_current()->pagedir, ptr
  ) == NULL){
    exit_process(-1);
  }
}

void check_buffer (void *buff_to_check, unsigned size)
{
  unsigned i;
  void *ptr  = buff_to_check;
  for (i = 0; i < size; i++)
    {
      check_ptr((const void *) ptr);
      ptr++;
    }
}

void exit_process (int status) {
  printf("%s: exit(%d)\n",thread_current()->name,status);
  //printf("%d exit_processing\n",thread_current()->tid);
  thread_current()->exit = status;
  thread_exit ();
}

int open (const char *file) {
  lock_acquire(&filesys_lock);
  struct file* fp = filesys_open(file);
  if (fp == NULL){
    return -1;
  }
  struct file_object *fo = malloc(sizeof(struct file_object)); 
  fo->f = fp;
  fo->fd = thread_current()->last_fd;
  fo->is_open = 1;
  enum intr_level old_level;
  old_level = intr_disable ();
  thread_current()->fdt = realloc(thread_current()->fdt,sizeof(struct file_object)*(thread_current()->last_fd+1));
  (thread_current()->fdt)[thread_current()->last_fd] = fo;
  thread_current()->last_fd = thread_current()->last_fd+1;
  intr_set_level (old_level);
  lock_release(&filesys_lock);
  return fo->fd;
}

void close (int fd) {
  if (fd == 0 || fd > thread_current()->last_fd || fd < 0){
    exit_process(-1);
  }
  lock_acquire(&filesys_lock);
  if (thread_current()->fdt[fd]->is_open == 1){
    file_close(thread_current()->fdt[fd]->f);
  }
  lock_release(&filesys_lock);
  thread_current()->fdt[fd]->is_open = 0;
}

int read (int fd, void *buffer, unsigned size) {
  if (fd == 0){
    uint8_t chr;
    uint8_t *buffer_ptr = (uint8_t *)buffer;
    for (int i = 0; i < (int)size; i++){
      chr = input_getc();
      *(buffer_ptr + i) = chr;
    }
    return (int)size;
  }else if (fd == 1){
    return -1;
  }else{
    int x = (int)file_read(thread_current()->fdt[fd]->f, buffer, size);
    return x;
  }
}

int filesize (int fd) {
  return (int)file_length(thread_current()->fdt[fd]->f);
}


/*
 int wait (pid_t pid) {

 }

bool create (const char *file, unsigned initial_size) {

}

bool remove (const char *file) {

}

int open (const char *file) {

}

int filesize (int fd) {

}

int read (int fd, void *buffer, unsigned size) {

}



void seek (int fd, unsigned position) {

}

unsigned tell (int fd) {

}

void close (int fd) {

}
*/


