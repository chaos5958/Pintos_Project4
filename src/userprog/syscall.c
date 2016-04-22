#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

//team 10
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

typedef int pid_t;
#define FD_START 0 

struct file_fd 
{
    struct file* file;
    int fd;
    struct list_elem fd_elem;
    struct list_elem fd_thread; 
};

static struct list file_list;
static struct lock file_lock; 

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
static struct file* find_file (int fd); 

static int get_fd (void);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init (&file_list);
  lock_init (&file_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *ptr = f->esp; 

  if (!is_user_vaddr (ptr))  
      goto done;
  
  if (!pagedir_get_page (thread_current ()->pagedir, ptr))
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

      case SYS_READ:  if (!is_user_vaddr (ptr + 1) || !is_user_vaddr (ptr + 2)
			     || !is_user_vaddr (ptr + 3))
			 goto done;
		     else{
			 int ret = read (*(ptr + 1), *(ptr + 2), *(ptr + 3));
			 f->eax = ret;
			 break;
		     }

      case SYS_WRITE:  
		       if (!is_user_vaddr (ptr + 1) || !is_user_vaddr (ptr + 2)
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
  return ;
done:
  exit (-1);
}

static void
halt (void) 
{
    power_off();
}

//static void

void
exit_ext (int status)
{ 
    struct thread* t = thread_current();
    //if parent waits(exists)
    if (t->parent != NULL){
    //give status to parent
      t->parent->ret_valid = true;
    }
    t->ret_status = status;
    thread_exit();
}

static void
exit (int status)
{ 
    struct thread* t = thread_current();
    //if parent waits(exists)
    if (t->parent != NULL){
    //give status to parent
      t->parent->ret_valid = true;
    }
    t->ret_status = status;
    thread_exit();
}
static pid_t
exec (const char *file)
{
    tid_t tid;

//    if (!is_user_vaddr (file) || file == NULL
//	    || !pagedir_get_page (thread_current ()->pagedir, file))
    if (file == NULL || !pagedir_get_page (thread_current ()->pagedir, file) || !is_user_vaddr (file))
      	exit(-1);
    lock_acquire (&file_lock);
    tid = process_execute (file);    
    lock_release (&file_lock);

    return (pid_t) tid;
}

static int
wait (pid_t pid)
{
    return process_wait ((tid_t)pid);
}

static bool
create (const char *file, unsigned initial_size)
{
//    if (!is_user_vaddr (file) || file == NULL
//	    || !pagedir_get_page (thread_current ()->pagedir, file))
      if(file == NULL || !pagedir_get_page (thread_current ()->pagedir, file))  
	exit (-1);
    

    return filesys_create (file, initial_size);
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
    int ret = -1; 
//    if (!is_user_vaddr (file) || file == NULL ||
//	    !pagedir_get_page (thread_current ()->pagedir, file))
    if(file == NULL || !pagedir_get_page (thread_current ()->pagedir, file))
	exit (-1);

    struct file* file_ = filesys_open (file);
    if (file_ == NULL) 
	goto done; 

    struct file_fd* fd_ = (struct file_fd*) malloc (sizeof (struct file_fd));
    if (fd_ == NULL)
	goto done;

    fd_->file = file_; 
    fd_->fd = get_fd();
    list_push_back (&file_list, &fd_->fd_elem);
    list_push_back (&thread_current()->open_file, &fd_->fd_thread);
    ret = fd_->fd; 

done:
   return ret;

}

static int
filesize (int fd) 
{
    int ret = -1;
    struct file_fd* file_fd; 
    struct list_elem* el;

    for (el = list_begin (&file_list); el != list_end (&file_list);
	   el = list_next (el))
    	{
	    file_fd = list_entry (el, struct file_fd, fd_elem);
    	    if (file_fd->fd == fd && file_fd->file != NULL)
	    {
            	ret = file_length (file_fd->file);
	    }

       }

    return ret;
}

static int
read (int fd, void *buffer, unsigned size)
{
    int ret = -1;
    int iteration; 
    struct file* file = NULL;
    struct file_fd* file_fd; 
    struct list_elem* el;

    //    if (!is_user_vaddr (buffer + size) 
//	    || buffer == NULL || !pagedir_get_page (thread_current ()->pagedir, buffer))
     
    if (buffer == NULL || !is_user_vaddr (buffer + size) || !pagedir_get_page (thread_current ()->pagedir, buffer))
    	exit (-1);

    lock_acquire (&file_lock);
    if (fd == STDIN_FILENO)
    {
	for (iteration = 0; iteration < size; iteration++)
	{
	   ((uint8_t *) buffer)[iteration] = input_getc();
	}
       	ret = size; 	
	goto done;
    }

    else if (fd == STDOUT_FILENO)
	goto done;

    else{
	
       	for (el = list_begin (&file_list); el != list_end (&file_list);
	   el = list_next (el))
    	{
	    file_fd  = list_entry (el, struct file_fd, fd_elem);
    	    if (file_fd->fd == fd && file_fd->file != NULL)
	    {
            	ret = file_read (file_fd->file, buffer, size);
		goto done;
	    }

       }
       /*
	if ((file = find_file (fd)) == NULL)
	    goto done;
	else
	    ret = file_read (file, buffer, size);	
	    */
    }


done:
    lock_release (&file_lock);
    return ret;
}

static int
write (int fd, const void *buffer, unsigned size)
{
    int ret = -1;
    int iteration; 
    struct file* file = NULL;
    struct file_fd* file_fd; 
    struct list_elem* el;
    
    //exit (-1);
    //    if (!is_user_vaddr (buffer + size) || buffer == NULL
//	    || !pagedir_get_page (thread_current ()->pagedir, buffer))
    if(buffer == NULL || !is_user_vaddr (buffer + size) || !pagedir_get_page (thread_current ()->pagedir, buffer + size)) 
      	  exit (-1);

    if (fd == 1) {
	putbuf (buffer, size);
	return size;
    }

    lock_acquire (&file_lock);
    if (fd == STDIN_FILENO)
	goto done;

    else if (fd == STDOUT_FILENO)
    {
	putbuf (buffer, size);
	ret = size;
    }
    
    else{
	
       	for (el = list_begin (&file_list); el != list_end (&file_list);
	   el = list_next (el))
    	{
	    file_fd  = list_entry (el, struct file_fd, fd_elem);
    	    if (file_fd->fd == fd && file_fd->file != NULL)
	    {
            	ret = file_write (file_fd->file, buffer, size);
		goto done;
	    }

       }
	/*	
	if ((file = find_file (fd)) == NULL)
	    goto done;
	else
	    ret = file_write (file, buffer, size);

	*/
    }

done:
    lock_release (&file_lock);
    return ret;
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
    struct file_fd* fd_ = NULL;
    struct thread* curr = thread_current();
    struct list_elem* el;
    
    for (el = list_begin (&curr->open_file) ;  el != list_end (&curr->open_file) ;
	     el = list_next (el))
    {
	fd_ = list_entry (el, struct file_fd, fd_thread);
	if (fd_->fd == fd) 
	    break; 
	else
	    fd_ = NULL;
    }

    if (fd_ == NULL) 
	exit (-1);

    /*
    for (el = list_begin (&file_list); el != list_end (&file_list);
	   el = list_next (el))
    	{
	    fd_ = list_entry (el, struct file_fd, fd_elem);
    	    if (fd_->fd == fd && fd_->file != NULL)
	    {		
		list_remove (el);
		file_close (fd_->file);
		free (fd_);
		break;
	    }
	}
	
    
    for (el = list_begin (&curr->open_file); el != list_end (&curr->open_file);
	   el = list_next (el))
    	{
	    fd_ = list_entry (el, struct file_fd, fd_thread);
    	    if (fd_->fd == fd && fd_->file != NULL)
	    {		
		list_remove (el);
		file_close (fd_->file);
		free (fd_);
		return ; 
	    }

	    else if (fd_->fd == fd && fd_->file == NULL)
		exit (-1);
	}
    exit (-1);
    */

    list_remove (&fd_->fd_elem);
    list_remove (&fd_->fd_thread);
    file_close (fd_->file);
    free (fd_);

}

static int
get_fd (void)
{
    static int current_fd = 2; 
    current_fd++; 
    return current_fd - 1;
}

static struct file* find_file (int fd)
{
    struct list_elem* el;
    struct thread* curr = thread_current ();
    struct file_fd* f_fd; 
    for (el = list_begin (&curr->open_file) ;  el != list_end (&curr->open_file) ;
	     el = list_next (el))
    {
	f_fd = list_entry (el, struct file_fd, fd_thread);
	if (f_fd == fd)
	    return f_fd->file;
    }
    return NULL;
}

