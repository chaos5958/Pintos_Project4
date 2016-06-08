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
struct inode *filesys_open_inode (const char*);

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

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size)
{
  disk_sector_t inode_sector = 0;
  struct dir *dir = get_dir (name);
  char *fname = get_name (name);
  bool success = false;
  if (strcmp(fname, ".") && strcmp(fname, ".."))
    success = (dir != NULL
	&& free_map_allocate (1, &inode_sector)
	&& inode_create (inode_sector, initial_size, false)
	&& dir_add (dir, fname, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

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
  struct dir *dir = get_dir(name);
  char *fname = get_name (name);
  bool success = false;
  if (!strcmp(fname, "."))
    printf("FILESYS_REMOVE: go to parent and remove this directory\n");
  else if (!strcmp(fname, ".."))
    printf("FILESYS_REMOVE: go to grandparent and remove parent directory\n");
  else
    success = ((dir != NULL) && dir_remove (dir, fname));
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

char *
get_name (const char *dirname)
{
  char *copy, *next, *name, *save_ptr;
  size_t dirnamelen = strlen(dirname) + 1;
  copy = (char*)calloc(1, dirnamelen);
  strlcpy(copy, dirname, dirnamelen);

  next = strtok_r(copy, "/", &save_ptr);
  name = next;
  while((next = strtok_r(NULL, "/", &save_ptr)))
    name = next;

  return name;
}

bool filesys_create_dir (const char *name, off_t initial_size)
{
  disk_sector_t inode_sector = 0;
  struct dir *dir = get_dir (name);
  char *fname = get_name (name);
  bool success = false;
  if (strcmp(fname, ".") && strcmp(fname, ".."))
    success = (dir != NULL
	&& free_map_allocate (1, &inode_sector)
	&& inode_create (inode_sector, initial_size, true)
	&& dir_add (dir, fname, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

struct dir *filesys_open_dir (const char *name)
{
  struct inode *inode = filesys_open_inode (name);
  return dir_open (inode);
}

struct inode *filesys_open_inode (const char *name)
{
  struct dir *dir = get_dir (name);
  struct inode *inode = NULL;
  char *fname = get_name (name);

  if (dir != NULL){
    if (!strcmp(fname, "."))
      inode = dir_get_inode(dir);
    else if (!strcmp(fname, ".."))
      printf("FILESYS_OPEN: open parent directory\n");
    else
      dir_lookup(dir, fname, &inode);
  }
  dir_close (dir);

  return inode;
}
