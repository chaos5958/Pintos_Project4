#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

//team 10
#include "threads/init.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "filesys/directory.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "lib/string.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"

typedef int pid_t; //process ID
#define FD_START 0 

struct file_fd //file descriptor structure
{
  struct file* file;
  struct dir* dir;
  bool is_dir; //whether it is file or directory that this structure is holding
  int fd; //file descriptor
  struct list_elem fd_elem; //list of all fd struct
  struct list_elem fd_thread; //list of fd struct that thread owns
};

static struct list file_list; //list of open files
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
static bool chdir (const char *dir);
static bool mkdir (const char *dir);
static bool readdir (int fd, char *name);
static bool isdir (int fd);
static int inumber (int fd);

static int get_fd (void);
static struct file_fd *find_fd (int fd);

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
		     pid_t ret = exec ((char *)(*(ptr + 1)));
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
		       bool ret = create ((char *)(*(ptr + 1)), *(ptr + 2));
		       f->eax = ret;
		       break;
		     }

    case SYS_REMOVE: if (!is_user_vaddr (ptr + 1))
		       goto done;
		     else{
		       bool ret = remove ((char *)(*(ptr + 1)));
		       f->eax = ret;
		       break;
		     }

    case SYS_OPEN: if (!is_user_vaddr (ptr + 1))
		     goto done;
		   else{
		     int ret = open ((char *)(*(ptr + 1)));
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
		      int ret = read (*(ptr + 1), (void *)(*(ptr + 2)), *(ptr + 3));
		      f->eax = ret;
		      break;
		    }

    case SYS_WRITE:  
		    if (!is_user_vaddr (ptr + 1) || !is_user_vaddr (ptr + 2)
			|| !is_user_vaddr (ptr + 3))
		      goto done;		        
		    else{
		      int ret = write (*(ptr + 1), (void *)(*(ptr + 2)), *(ptr + 3));
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

    case SYS_CHDIR: if (!is_user_vaddr (ptr + 1))
		      goto done;
		    else{
		      bool ret = chdir((char *)(*(ptr + 1)));
		      f->eax = ret;
		      break;
		    }
    case SYS_MKDIR: if (!is_user_vaddr (ptr + 1))
		      goto done;
		    else{
		      bool ret = mkdir((char *)(*(ptr + 1)));
		      f->eax = ret;
		      break;
		    }
    case SYS_READDIR:
		    if (!is_user_vaddr (ptr + 1) || !is_user_vaddr (ptr + 2))
		      goto done;
		    else{
		      bool ret = readdir(*(ptr + 1), (char *)(*(ptr + 2)));
		      f->eax = ret;
		      break;
		    }
    case SYS_ISDIR: if (!is_user_vaddr (ptr + 1))
		      goto done;
		    else{
		      bool ret = isdir(*(ptr + 1));
		      f->eax = ret;
		      break;
		    }
    case SYS_INUMBER:
		    if (!is_user_vaddr (ptr + 1))
		      goto done;
		    else{
		      int ret = inumber(*(ptr + 1));
		      f->eax = ret;
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
  //if parent waits(exists) ret_valid is true.
  if (t->parent != NULL){
    t->parent->ret_valid = true;
  }
  t->ret_status = status;
  thread_exit();
}

static pid_t
exec (const char *file)
{
  tid_t tid;

  if (file == NULL || !is_user_vaddr (file) || !pagedir_get_page (thread_current ()->pagedir, file) || !is_user_vaddr (file))
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
  if(file == NULL ||!is_user_vaddr (file) || !pagedir_get_page (thread_current ()->pagedir, file))  
    exit (-1);

  return filesys_create (file, initial_size);
}

static bool
remove (const char *file)
{
  if (file == NULL || !is_user_vaddr (file) || !pagedir_get_page (thread_current ()->pagedir, file))
    exit(-1);

  return filesys_remove (file);
}

/* Open directory or file with name FILE
 * Returns its file descriptor, -1 on failure
 * Makes file descriptor structure
 * Fails if directory or file with name FILE doesn't exist
 * or internal memory allocation fails*/
static int
open (const char *file)
{
  int ret = -1; 

  if(file == NULL || !is_user_vaddr (file) || !pagedir_get_page (thread_current ()->pagedir, file))
    exit (-1);

  /* set directory and file */
  struct dir* dir_ = NULL;
  struct file* file_ = NULL;
  struct inode* inode_ = filesys_open_inode (file);
  bool is_dir;

  /* according to inode info, open dir struct or file*/
  if (inode_is_dir (inode_))
  {     
      dir_ = dir_open(inode_);
      is_dir = true;
  }
  else{
      file_ = file_open(inode_);
      is_dir = false;
  }
  if ((file_ == NULL) && (dir_ == NULL))
      goto done;

  /* allocate file_fd */
  struct file_fd* fd_ = (struct file_fd*) malloc (sizeof (struct file_fd));
  if (fd_ == NULL)
    goto done;

  /* save file_fd and push to list */
  fd_->file = file_;
  fd_->dir = dir_;
  fd_->is_dir = is_dir;
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
  unsigned int iteration; 
  struct file_fd* file_fd; 
  struct list_elem* el;

  if (buffer == NULL || !is_user_vaddr (buffer + size) || !pagedir_get_page (thread_current ()->pagedir, buffer))
    exit (-1);

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

  else
  {
    for (el = list_begin (&file_list); el != list_end (&file_list);
	el = list_next (el))
    {
      file_fd  = list_entry (el, struct file_fd, fd_elem);
      if ((file_fd->fd == fd) && (file_fd->file != NULL))
      {
	ret = file_read (file_fd->file, buffer, size);
	goto done;
      }
    }
  }

done:
  return ret;
}

static int
write (int fd, const void *buffer, unsigned size)
{
  int ret = -1;
  struct file_fd* file_fd; 
  struct list_elem* el;

  if(buffer == NULL || !is_user_vaddr (buffer + size) || !pagedir_get_page (thread_current ()->pagedir, buffer + size)) {
    exit (-1);
  }

  if (fd == 1) {
    putbuf (buffer, size);
    return size;
  }

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
      if ((file_fd->fd == fd) && (file_fd->file != NULL))
      {
	  if (file_fd->is_dir)
	      return -1;
	  else
	  {
	      ret = file_write (file_fd->file, buffer, size);
	      goto done;
	  }
      }

    }

  }

done:
  return ret;
}

static void
seek (int fd, unsigned position) 
{
  struct file* file;
  struct file_fd* file_fd;
  struct list_elem* el; 

  for (el = list_begin (&file_list); el != list_end (&file_list);
      el = list_next (el))
  {
    file_fd  = list_entry (el, struct file_fd, fd_elem);
    if (file_fd->fd == fd && file_fd->file != NULL)
    {
      file = file_fd->file;
      file_seek (file, position);
      return ;
    }
  }
}

static unsigned
tell (int fd) 
{
  struct file_fd* f_fd;
  struct list_elem* el;

  for (el = list_begin (&file_list); el != list_end (&file_list);
      el = list_next (el)){
    f_fd = list_entry (el, struct file_fd, fd_elem);
    if (f_fd->fd == fd && f_fd->file != NULL){
      return file_tell (f_fd->file);
    }
  }
  PANIC("no such file");
}

/* Close file or dir struct of file discriptor FD */
static void
close (int fd)
{
  /* find file descriptor structure */
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

  /* remove from list, close, free */
  list_remove (&fd_->fd_elem);
  list_remove (&fd_->fd_thread);
  file_close (fd_->file);
  dir_close (fd_->dir);
  free (fd_);
}

static int
get_fd (void)
{
  static int current_fd = 2; 
  current_fd++; 
  return current_fd - 1;
}

/* Closefile or dir struct that has list_elem EL_ */
void close_file (struct list_elem* el_)
{
  ASSERT (el_ != NULL);

  /* get file descriptor structure */
  struct file_fd* f_fd = NULL;
  f_fd = list_entry (el_, struct file_fd, fd_thread);

  /* remove from list, close, free*/
  list_remove (&f_fd->fd_elem);
  file_close (f_fd->file);
  dir_close (f_fd->dir);
  free (f_fd);
}

/* Change directory of current process to DIR 
 * return true if successful, false on failure
 * failure on internal memory problem or unexisting DIR*/
static bool chdir (const char *dir)
{
  /* set strings: add {dummy directory}, so that get_dir regards given dir as directory to return and {added dummy directory} as file that exists in returned directory */
  size_t dirlen = strlen(dir);
  char *copy = (char*)calloc(1, dirlen + 3);
  if (!copy)
    return false;
  strlcpy(copy, dir, dirlen + 1);
  copy[dirlen] = '/';
  copy[dirlen + 1] = '.';
  copy[dirlen + 2] = '\0';

  /* get directory, close current process directory, set new directory */
  struct dir *directory = get_dir(copy);
  if (!directory){
    return false;
  }
  dir_close(thread_current()->dir);
  thread_current()->dir = directory;
  free(copy);
  return true;
}

/* Create directory DIR
 * Return true if successful, false on failure
 * Directory upper to the directory that is to be created must exist
 * example: a/b/c fails if a/b does not exist*/
static bool mkdir (const char *dir)
{
  if (!dir)
    return false;
  return filesys_create_dir(dir, 0);
}

/* Read file or directory from directory given as FD
 * read file or direcctory name on NAME
 * return true if file or dir exists, false if not*/
static bool readdir (int fd, char *name)
{
  /* Get file descriptor structure */
  struct file_fd *f_fd = find_fd(fd);
  if (!f_fd){
    return false;
  }
  
  /* Get directory structure */
  struct dir *dir = f_fd->dir;
  if (!dir){
    return false;
  }
  /* Read from directory */
  return dir_readdir(dir, name);
}

/* If fd is directory return true
 * If fd is not directory return false
 * If fd is wrong descriptor asserts */
static bool isdir (int fd)
{
  struct file_fd *f_fd = find_fd (fd);
  //ASSERT (f_fd != NULL); 

  return f_fd->is_dir;
}

/* Return inode number of inode of dir struct or file of file descriptor */
static int inumber (int fd)
{
  struct file_fd  *f_fd = find_fd (fd);

  if (f_fd == NULL)
      return -1;

  if (f_fd->is_dir)
    return inode_get_inumber (dir_get_inode (f_fd->dir));
  return inode_get_inumber (file_get_inode (f_fd->file));
}

/* find file descriptor structure with file descriptor */
static struct file_fd *find_fd (int fd)
{
  struct list_elem *e;
  for (e = list_begin (&file_list); e != list_end (&file_list);
      e = list_next(e)){
    struct file_fd *f_fd = list_entry(e, struct file_fd, fd_elem);
  
    if (f_fd->fd == fd)
      return f_fd;
  }
  return NULL;
}
