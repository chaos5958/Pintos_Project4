#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

#define NAME_LEN_MAX 196

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);
static void get_name_prev (char *name);
struct lock create_lock;

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();
  init_cache ();
  if (format) 
    do_format ();
  lock_init(&create_lock);

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  cache_to_disk (); 
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  size_t namelen = strlen (name) + 1;
  char *copy = (char *) malloc (namelen);
  strlcpy (copy, name, namelen);

  disk_sector_t inode_sector = 0;
  struct dir *dir = get_dir (name);
  char *fname = get_name (copy);
  bool success = false;
  lock_acquire(&create_lock);
  if (strcmp(fname, ".") && strcmp(fname, ".."))
      success = (dir != NULL
	&& free_map_allocate (1, &inode_sector)
	&& inode_create (inode_sector, initial_size, false)
	&& dir_add (dir, fname, inode_sector));

  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);
  lock_release(&create_lock);

  free (copy);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct inode *inode = filesys_open_inode (name);
  
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  if (strlen (name) == 0)
      return false;

  /* set and copy character NAME */
  char *name_ = (char *) malloc (strlen (name) + 1);
  memcpy (name_, name, strlen (name) + 1);

  /* set directory, inode, and other variables */
  char temp[NAME_MAX + 1]; //to check subdirectory exists
  struct dir *dir = NULL; //directory that object being removed is located
  struct dir *dir_rm = NULL; //directory that is being removed(unused when removing file)
  bool success = false; //result
  struct inode *inode; //to open additional directory in case 1, 2
  
  /* get directory that file or directory to remove is located at
   * and name of file or directory that is to be deleted*/
  dir = get_dir(name_);
  char *fname = get_name (name_);

  //case 1: a/b/NULL && / 
  if (fname == NULL)
  {
      //get inode of /a from /a/b/NULL
      if ((inode = get_parentdir (dir_get_inode (dir))) == NULL)	  
	  return success;

      //get directory struct of a and name b
      dir_rm = dir;
      dir = dir_open(inode);
      get_name_prev (name_);
   
      //if b empty, remove from a
      if (!dir_readdir (dir_rm, temp))
      {
	  lock_acquire(&create_lock);
	  success = ((dir != NULL) && dir_remove (dir, name_));
	  lock_release(&create_lock);
      } 
  }

  //case 2: a/b/.
  else if (!strcmp (fname, "."))
  {
      //get inode of a
      if ((inode = get_parentdir (dir_get_inode (dir))) == NULL)	  
	  return success;

      //get directory struct of a and name b
      dir_rm = dir;
      dir = dir_open(inode);
      get_name_prev (name_);

      //if b empty, remove from a
      if (!dir_readdir (dir_rm, temp))
      {
	  lock_acquire(&create_lock);
  	  success = ((dir != NULL) && dir_remove (dir, name_));
	  lock_release(&create_lock);
      }
  }
  //case 3: a/b/..
  else if (!strcmp (fname, ".."))
  {
      //try to remove a, but b exists in directory a, so fails
      return success;
  }

  //case 4: a/b/c
  else
  {
      //get inode of c
      dir_lookup(dir, fname, &inode);
      //c is directory
      if (inode_is_dir (inode))
      {
	  //open directory of c
	  dir_rm = dir_open (inode);
	  //if c empty, remove c from a/b
	  if (!dir_readdir (dir_rm, temp))
	  {
	      lock_acquire(&create_lock);
	      success = ((dir != NULL) && dir_remove (dir, fname));
	      lock_release(&create_lock);
	  } 
      }
      //c is file, remove c from a/b
      else
      {
	  lock_acquire(&create_lock);
       	  success = ((dir != NULL) && dir_remove (dir, fname));
	  lock_release(&create_lock);
      }
      inode_close (inode);
  }

  free (name_);
  dir_close (dir_rm);
  dir_close (dir); 

  return success; 
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* Returns name of file or directory from full name of total directory and file
 * for example, a/b/c -> c
 *              a/b/ -> NULL
 * (character ptr DIRNAME modified in this function)*/
char *
get_name (char *dirname)
{
  if (strlen (dirname) == 0)
      return dirname;

  char *next, *name, *save_ptr;

  next = strtok_r (dirname, "/", &save_ptr);
  name = next;
  while((next = strtok_r(NULL, "/", &save_ptr)))
      name = next;

  return name;
}

/* Creates directory NAME with given INITIAL_SIZE
 * Returns true on success, false on fail
 * Fails if a directory named NAME already exists or internal memory allocation failsi
 * (character ptr NAME unmodified in this function)*/
bool filesys_create_dir (const char *name, off_t initial_size)
{
    size_t namelen = strlen (name) + 1;
    char *copy = (char *) malloc (namelen);
    strlcpy (copy, name, namelen);

    disk_sector_t inode_sector = 0;
    struct dir *dir = get_dir (name);
    char *fname = get_name (copy);
    bool success = false;
    if (strcmp(fname, ".") && strcmp(fname, ".."))//create child directory
	success = (dir != NULL
		&& free_map_allocate (1, &inode_sector)
		&& inode_create (inode_sector, initial_size, true)
		&& dir_add (dir, fname, inode_sector));
    if (!success && inode_sector != 0)//failure
	free_map_release (inode_sector, 1);

    dir_close (dir);//close current directory
    free (copy);
    return success;
}

/* Opens directory structure DIR of given NAME
 * Returns NULL pointer on failure
 * Fails if no directory NAME exists, or if internal memory allocation fails
 * (character pointer input NAME unmodified in this function)*/
struct dir *filesys_open_dir (const char *name)
{
    struct inode *inode = filesys_open_inode (name);
    return dir_open (inode);
}

/* Opens inode structure of given NAME or a null pointer on failure
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails.
 * (character pointer input NAME unmodified in this function)*/
struct inode *filesys_open_inode (const char *name)
{
  size_t namelen = strlen (name) + 1;
  char *copy = (char *) malloc (namelen);
  strlcpy (copy, name, namelen);
  
  struct dir *dir = get_dir (name);
  struct inode *inode = NULL;
  char *fname = get_name (copy);
  if (dir != NULL){
    if (fname == NULL || !strcmp(fname, "."))
    {
      inode = inode_reopen (dir_get_inode(dir));
    }
    else if (!strcmp(fname, ".."));
    else
    {
      dir_lookup(dir, fname, &inode);
    }
  }
  dir_close (dir);
  free (copy);
  return inode;
}

/* get name of current directory of NAME
 * for instance,
 *   NAME a/b/c -> b
 *   NAME a/b/ -> b 
 * (character ptr NAME is modified in this function)*/
static void get_name_prev (char *name)
{
    size_t len = strlen (name);

    /* delete character from end until '/' appears */
    while (name[len] == '/')
    {
	name[len] = '\0';
	len--;
    }

    /* using get_name, get last subdirectory name of modified NAME */
    name[len] = '\0';
    name = get_name (name);
}

