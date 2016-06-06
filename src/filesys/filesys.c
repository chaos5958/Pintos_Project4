#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/thread.h"
#include "threads/malloc.h"

#define NAME_LEN_MAX 196

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);
struct dir *get_dir (const char*);
char *get_name (const char*);

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
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
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
	struct dir *dir = get_dir (name);
	struct inode *inode = NULL;
	char *fname = get_name (name);
	
	if (dir != NULL)
		dir_lookup(dir, fname, &inode);	
	dir_close (dir);
	
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
  bool success = dir != NULL && dir_remove (dir, fname);
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

struct dir *
get_dir (const char *dirfile)
{
	struct dir *dir = thread_current()->dir;
	struct inode *inode = NULL;

	if (!dirfile)
		return NULL;

	int dirfilelen = strlen(dirfile) + 1;
	char copy[dirfilelen], *sub, *subnext, *save_ptr;
	sub = (char*)calloc(1, NAME_MAX + 1);

	memcpy(copy, dirfile, dirfilelen);

	/*obtain topmost directory*/
	if ((dir) && (copy[0] != '/'))
		dir = dir_reopen(dir);
	else
		dir = dir_open_root();

	subnext = strtok_r(copy, "/", &save_ptr);
	memcpy(sub, subnext, NAME_MAX + 1);
	while ((subnext = strtok_r(NULL, "/", &save_ptr))){
		if (!strcmp(sub, "."));
		else if (!strcmp(sub, ".."))
			printf("  GET_DIR: go to parent directory\n");
		else{
			if (!dir_lookup(dir, sub, &inode)){
				dir_close(dir);
				free(sub);
				return NULL;
			}
			dir_close(dir);
			dir = dir_open(inode);
		}

		memcpy(sub, subnext, NAME_MAX + 1);
	}

	free(sub);
	return dir;
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
