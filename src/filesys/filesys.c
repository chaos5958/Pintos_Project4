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

#define NAME_LEN_MAX 196

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);
static void get_name_prev (char *name);

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
    //char *fname = get_name (name);
    char *fname = get_name (copy);
  bool success = false;
  if (strcmp(fname, ".") && strcmp(fname, ".."))
      success = (dir != NULL
	&& free_map_allocate (1, &inode_sector)
	&& inode_create (inode_sector, initial_size, false)
	&& dir_add (dir, fname, inode_sector));

  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

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
  //printf ("FILESYS_REMOVE: START\n");
   
  /* 
  struct dir *dir = get_dir(name);
  char *fname = get_name (name);
  bool success = false;
  if (!strcmp(fname, "."))
  {
  }

    //printf("FILESYS_REMOVE: go to parent and remove this directory\n");
  else if (!strcmp(fname, ".."))
  {
    //printf("FILESYS_REMOVE: go to grandparent and remove parent directory\n");
  }
  else
  {
      if (inode_is_dir (inode))
      {
	  dir_rm = dir_open (inode);
	  if (!dir_readdir (dir_rm, temp))
	  {
	      //printf ("ELSE FNAME:  %s\n", fname);
	      success = ((dir != NULL) && dir_remove (dir, fname));
	  } 
      }
    success = ((dir != NULL) && dir_remove (dir, fname));
  dir_close (dir); 

  //printf ("FILESYS_REMOVE: END\n");
  return success;
  */
  
  if (strlen (name) == 0)
      return false;

  char *name_ = (char *) malloc (strlen (name) + 1);
  memcpy (name_, name, strlen (name) + 1);

  char temp[NAME_MAX + 1];
  struct dir *dir = NULL;
  struct dir *dir_rm = NULL;
  bool success = false;
  struct inode *inode;
  
  dir = get_dir(name_);
  char *fname = get_name (name_);

  //printf ("fname: %s\n", fname); 
  //case 1: a/b/NULL && / 
  if (fname == NULL)
  {
      if ((inode = get_parentdir (dir_get_inode (dir))) == NULL)	  
	  return success;

      dir_rm = dir_open(inode);
      get_name_prev (name_);
   
      if (!dir_readdir (dir, temp))
      {
	  success = ((dir != NULL) && dir_remove (dir, name_));
      } 
  }

  //case 2: a/b/.
  else if (!strcmp (fname, "."))
  {
      if ((inode = get_parentdir (dir_get_inode (dir))) == NULL)	  
	  return success;

      dir_rm = dir_open(inode);
      get_name_prev (name_);
  }
  //case 3: a/b/..
  else if (!strcmp (fname, ".."))
  {
      return success;
  }

  //case 4: a/b/c
  else
  {
      //printf ("case 4\n");
      dir_lookup(dir, fname, &inode);
      if (inode_is_dir (inode))
      {
	  dir_rm = dir_open (inode);
	  if (!dir_readdir (dir_rm, temp))
	  {
	      //printf ("ELSE FNAME:  %s\n", fname);
	      success = ((dir != NULL) && dir_remove (dir, fname));
	  } 
      }
      else
      {
	  //printf ("file\n");
       	  success = ((dir != NULL) && dir_remove (dir, fname));
      }
      inode_close (inode);
  }

  free (name_);
  dir_close (dir_rm);
  dir_close (dir); 

  //printf ("end\n");
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

char *
get_name (char *dirname)
{
  if (strlen (dirname) == 0)
      return dirname;

//  char *copy, *next, *name, *save_ptr;
//  size_t dirnamelen = strlen (dirname) + 1;
//  copy = (char *)calloc (1, dirnamelen);
//  strlcpy (copy, dirname, dirnamelen);
//
//  next = strtok_r (copy, "/", &save_ptr);
//  name = next;

  char *next, *name, *save_ptr;

  next = strtok_r (dirname, "/", &save_ptr);
  name = next;
  while((next = strtok_r(NULL, "/", &save_ptr)))
      name = next;

  return name;
}

bool filesys_create_dir (const char *name, off_t initial_size)
{
    size_t namelen = strlen (name) + 1;
    char *copy = (char *) malloc (namelen);
    strlcpy (copy, name, namelen);


    disk_sector_t inode_sector = 0;
    struct dir *dir = get_dir (name);
    //char *fname = get_name (name);
    char *fname = get_name (copy);
    bool success = false;
    if (strcmp(fname, ".") && strcmp(fname, ".."))
	success = (dir != NULL
		&& free_map_allocate (1, &inode_sector)
		&& inode_create (inode_sector, initial_size, true)
		&& dir_add (dir, fname, inode_sector));

    if (!success && inode_sector != 0)
	free_map_release (inode_sector, 1);
    dir_close (dir);

    free (copy);
    return success;
}

struct dir *filesys_open_dir (const char *name)
{
    struct inode *inode = filesys_open_inode (name);
    return dir_open (inode);
}

struct inode *filesys_open_inode (const char *name)
{
  //printf ("FILESYS_OPEN_INODE START\n");
  
    size_t namelen = strlen (name) + 1;
    char *copy = (char *) malloc (namelen);
    strlcpy (copy, name, namelen);

  
  struct dir *dir = get_dir (name);
  struct inode *inode = NULL;
  //char *fname = get_name (name);
  char *fname = get_name (copy);
  //printf ("dir name: %s\n", name);
  //printf ("name: %s\n", fname);
  if (dir != NULL){
    if (fname == NULL || !strcmp(fname, "."))
    {
      //inode = dir_get_inode(dir);
      inode = inode_reopen (dir_get_inode(dir));
    }
    else if (!strcmp(fname, ".."))
    {
     // printf("FILESYS_OPEN: open parent directory\n");
    }
    else
    {
      dir_lookup(dir, fname, &inode);
      //dir_close (dir);
      //inode_close (inode);
    }
  }
  //printf ("FILESYS_OPEN_INODE END\n");
  //dir_close (dir);
  //if (fname != NULL)
  //    free (fname);
  dir_close (dir);
  free (copy);
  return inode;
}

struct inode *filesys_open_inode_test (const char *name)
{

  //printf ("FILESYS_OPEN_INODE_TEST START\n");
  struct dir *dir = get_dir (name);
  struct inode *inode = NULL;
  char *fname = get_name (name);
  
  //printf ("dir name: %s\n", name);
  //printf ("fname: %s\n", fname);
  if (dir != NULL){
    if (fname == NULL || !strcmp(fname, "."))
    {
      inode = dir_get_inode(dir);
    }
    else if (!strcmp(fname, ".."))
    {
     // printf("FILESYS_OPEN: open parent directory\n");
    }
    else
    {
	//printf ("ELSE\n");
      dir_lookup(dir, fname, &inode);
      dir_close (dir);
    }
    //dir_close (dir);
  }
  //printf ("FILESYS_OPEN_INODE_TEST END\n");
  //free (fname);
 // return NULL;
  return inode;
}

static void get_name_prev (char *name)
{
    size_t len = strlen (name);

    while (name[len] == '/')
    {
	name[len] = '\0';
	len--;
    }

    name[len] = '\0';
    name = get_name (name);
}

