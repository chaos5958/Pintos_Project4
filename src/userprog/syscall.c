#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

//team 10
#include "threads/vaddr.h"

typedef int pid_t;

static void syscall_handler (struct intr_frame *);

static void halt (void);
static void exit (int status);
static pid_t exec (const char *file);
static int wait (pid_t);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned length);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *ptr = f->esp; 

  if (!is_user_vaddr (ptr))  
      goto done;
  
  if (*ptr < SYS_HALT || *ptr > SYS_INUMBER)
      goto done;
 
  switch (*ptr)
  {
      case SYS_HALT: halt();
		     break;
      case SYS_EXIT: if (!is_user_vaddr (ptr + 1))
    			 goto done;
		 else{
		     exit (*(ptr + 1));
		     break;
		 }

      case SYS_EXEC: if (!is_user_vaddr (ptr + 1))
			 goto done;
		     else{
			 pid_t ret = exec (*(ptr + 1));
			 f->eax = ret;
			 break;
		     }

      case SYS_WAIT: if (!is_user_vaddr (ptr + 1))
			 goto done;
		     else{
			 int ret = wait (*(ptr + 1));
			 f->eax = ret;
			 break;
		     }

      case SYS_CREATE: if (!is_user_vaddr (ptr + 1) || !is_user_vaddr (ptr + 2))
			   goto done;
		       else{
			   bool ret = create (*(ptr + 1), *(ptr + 2));
			   f->eax = ret;
		           break;
		       }

      case SYS_REMOVE: if (!is_user_vaddr (ptr + 1))
		           goto done;
		       else{
			   bool ret = remove (*(ptr + 1));
			   f->eax = ret;
			   break;
		       }

      case SYS_OPEN: if (!is_user_vaddr (ptr + 1))
			 goto done;
		     else{
			 int ret = open (*(ptr + 1));
			 f->eax = ret;
			 break;
		     }

      case SYS_FILESIZE: if (!is_user_vaddr (ptr + 1))
			     goto done;
			 else{
			     int ret = filesize (*(ptr + 1));
			     f->eax = ret;
			     break;
			 }

      case SYS_READ: if (!is_user_vaddr (ptr + 1) || !is_user_vaddr (ptr + 2)
			     || !is_user_vaddr (ptr + 3))
			 goto done;
		     else{
			 int ret = read (*(ptr + 1), ptr + 2, *(ptr + 3));
			 f->eax = ret;
			 break;
		     }

      case SYS_WRITE: if (!is_user_vaddr (ptr + 1) || !is_user_vaddr (ptr + 2)
			      || !is_user_vaddr (ptr + 3))
			  goto done;
		      else{
			  int ret = write (*(ptr + 1), *(ptr + 2), *(ptr + 3));
			  f->eax = ret;
			  break;
		      }

      case SYS_SEEK: if(!is_user_vaddr (ptr + 1) || !is_user_vaddr (ptr + 2))
			 goto done;
		     else{
			 seek (*(ptr + 1), *(ptr + 2));
			 break;
		     }

      case SYS_TELL: if(!is_user_vaddr (ptr + 1))
			 goto done;
		     else{
			 unsigned ret = tell (*(ptr + 1));
			 f->eax = ret;
			 break;
		     }

      case SYS_CLOSE: if(!is_user_vaddr (ptr + 1))
			   goto done;
		       else{
			   close (*(ptr + 1));
			   break;
		       }
  }

  //thread_exit();
  return ;

done:
  printf("unvalid vaddr\n");
  return; 
  //////////////////LATER, CHANGE TO EXIT 
}

static void
halt (void) 
{
    printf("halt\n");
}

static void
exit (int status)
{
    struct thread* t = thread_current();
    //if parent waits(exists)
    if (t->parent != NULL){
    //give status to parent
      t->parent->ret_status = status;
      t->parent->ret_valid = true;
    }
    thread_exit();
}

static pid_t
exec (const char *file)
{
    printf("halt\n");
    return 0;
}

static int
wait (pid_t pid)
{
    printf("wait\n");
    return 0;
}

static bool
create (const char *file, unsigned initial_size)
{
    printf("create\n");
    return 0;
}

static bool
remove (const char *file)
{
    printf("remove\n");
    return 0;
}

static int
open (const char *file)
{
    printf("open\n");
    return 0;
}

static int
filesize (int fd) 
{
    printf("filesize\n");
    return 0;
}

static int
read (int fd, void *buffer, unsigned size)
{
    printf("read\n");
    return 0;
}

static int
write (int fd, const void *buffer, unsigned size)
{
    //printf("write\n");
    if (fd == 1){
	putbuf (buffer, size);
	return size;
    }


    return 0;
}

static void
seek (int fd, unsigned position) 
{
    printf("seek\n");
    return 0;
}

static unsigned
tell (int fd) 
{
    printf("tell\n");
    return 0; 
}

static void
close (int fd)
{
    printf("close\n");
}

