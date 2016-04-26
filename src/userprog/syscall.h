#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
void syscall_init (void);

void exit_ext (int status);
void close_file (struct list_elem*);

#endif /* userprog/syscall.h */
