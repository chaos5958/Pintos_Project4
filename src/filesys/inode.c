#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* imbalanced design */
#define DIRECT_PTR_NUM 12
#define INDIRECT_PTR_NUM 4
#define DOUBLE_INDIRECT_PTR_NUM 1
#define TOTAL_PTR_NUM (DIRECT_PTR_NUM + INDIRECT_PTR_NUM + DOUBLE_INDIRECT_PTR_NUM)
#define UNUSED_NUM (123 - TOTAL_PTR_NUM)
#define INDIRECT_BLOCK_SIZE (DISK_SECTOR_SIZE / 4)

#define ERROR_ALLOC -1

/* for debugging */
#define OLD 0

//struct inode_disk
//  {
//    disk_sector_t start;                /* First data sector. */
//    off_t length;                       /* File size in bytes. */
//    unsigned magic;                     /* Magic number. */
//    uint32_t unused[125];               /* Not used. */
//  };

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
{
    off_t length;
    off_t direct_idx;
    off_t indirect_idx;
    off_t double_indirect_idx;
    unsigned magic;
    disk_sector_t directory [TOTAL_PTR_NUM];
    uint32_t unused[UNUSED_NUM];
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}
static bool inode_allocate (size_t, struct inode_disk *);
static int indirect_allocate (size_t, disk_sector_t);
static int double_indirect_allocate (size_t, disk_sector_t);
static void inode_deallocate (struct inode_disk *);
static void indirect_deallocate (disk_sector_t, size_t);
static void double_indirect_deallocate (disk_sector_t, size_t, size_t);
static off_t expand_file (struct inode *, off_t);

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
/* team10
 * impl indirected block based calculation
 */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
  {
      int idx = pos / DISK_SECTOR_SIZE;

      if (idx < DIRECT_PTR_NUM)
      {
	  return inode->data.directory[idx];
      }

      idx -= DIRECT_PTR_NUM;

      if (idx < INDIRECT_PTR_NUM * INDIRECT_BLOCK_SIZE)
      {
	  disk_sector_t block [INDIRECT_BLOCK_SIZE];
	  disk_read (filesys_disk, inode->data.directory[idx / INDIRECT_BLOCK_SIZE + DIRECT_PTR_NUM], block);
	  
	  return block[idx % INDIRECT_BLOCK_SIZE];
      }

      idx -= INDIRECT_PTR_NUM * INDIRECT_BLOCK_SIZE;

      if (idx < INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_SIZE)
      {
	  disk_sector_t block [INDIRECT_BLOCK_SIZE];
	  
	  disk_read (filesys_disk, inode->data.directory[DIRECT_PTR_NUM + INDIRECT_PTR_NUM], block);
	  disk_read (filesys_disk, block[idx / INDIRECT_BLOCK_SIZE], block);

	  return block[idx % INDIRECT_BLOCK_SIZE];
      }

      return -1;
   }
  else
      return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;

      if (inode_allocate (sectors, disk_inode))
      {
	  disk_write (filesys_disk, sector, disk_inode);
	  success = true;
      }

      free (disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
    struct inode *
inode_open (disk_sector_t sector) 
{
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
	    e = list_next (e)) 
    {
	inode = list_entry (e, struct inode, elem);
	if (inode->sector == sector) 
	{
	    inode_reopen (inode);
	    return inode; 
	}
    }

    /* Allocate memory. */
    inode = malloc (sizeof *inode);
    if (inode == NULL)
	return NULL;

    /* Initialize. */
    list_push_front (&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    disk_read (filesys_disk, inode->sector, &inode->data);
    //printf("    INODE_OPEN: INODE %d DENY %d\n", inode, inode->deny_write_cnt);
    return inode;
}

/* Reopens and returns INODE. */
    struct inode *
inode_reopen (struct inode *inode)
{
    if (inode != NULL)
	inode->open_cnt++;
    return inode;
}

/* Returns INODE's inode number. */
    disk_sector_t
inode_get_inumber (const struct inode *inode)
{
    return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
    void
inode_close (struct inode *inode) 
{
    /* Ignore null pointer. */
    if (inode == NULL)
	return;

    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0)
    {
	/* Remove from inode list and release lock. */
	list_remove (&inode->elem);

	/* Deallocate blocks if removed. */
	if (inode->removed) 
	{
	    free_map_release (inode->sector, 1);
	    inode_deallocate (&inode->data);
//	    free_map_release (inode->data.start,
//		    bytes_to_sectors (inode->data.length)); 
	}
	else
	{
	    /* a file is closed but it is remained in a directory */ 
	}

	free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
    void
inode_remove (struct inode *inode) 
{
    ASSERT (inode != NULL);
    inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
    off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;
    uint8_t *bounce = NULL;

    while (size > 0) 
    {
	/* Disk sector to read, starting byte offset within sector. */
	disk_sector_t sector_idx = byte_to_sector (inode, offset);
	int sector_ofs = offset % DISK_SECTOR_SIZE;

	/* Bytes left in inode, bytes left in sector, lesser of the two. */
	off_t inode_left = inode_length (inode) - offset;
	int sector_left = DISK_SECTOR_SIZE - sector_ofs;
	int min_left = inode_left < sector_left ? inode_left : sector_left;

	/* Number of bytes to actually copy out of this sector. */
	int chunk_size = size < min_left ? size : min_left;
	if (chunk_size <= 0)
	    break;

	read_cache (sector_idx, buffer + bytes_read, chunk_size, sector_ofs);

	/* Advance. */
	size -= chunk_size;
	offset += chunk_size;
	bytes_read += chunk_size;
    }
    free (bounce);

    return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
    off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
	off_t offset) 
{
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;
    uint8_t *bounce = NULL;
    //printf("  INODE_WRITE_AT: INODE %d DENY %d\n", inode, inode->deny_write_cnt);
    if (inode->deny_write_cnt)
	return 0;

    if (inode->data.length < offset + size)
    {
	//printf ("expand file\n");
	inode->data.length = expand_file (inode, offset + size);
	disk_write (filesys_disk, inode->sector, &inode->data);
    }

    while (size > 0) 
    {
	/* Sector to write, starting byte offset within sector. */
	disk_sector_t sector_idx = byte_to_sector (inode, offset);
	int sector_ofs = offset % DISK_SECTOR_SIZE;

	/* Bytes left in inode, bytes left in sector, lesser of the two. */
	off_t inode_left = inode_length (inode) - offset;
	int sector_left = DISK_SECTOR_SIZE - sector_ofs;
	int min_left = inode_left < sector_left ? inode_left : sector_left;

	/* Number of bytes to actually write into this sector. */
	int chunk_size = size < min_left ? size : min_left;
	if (chunk_size <= 0)
	    break;

	write_cache (sector_idx, buffer + bytes_written, chunk_size, sector_ofs);

	/* Advance. */
	size -= chunk_size;
	offset += chunk_size;
	bytes_written += chunk_size;
    }
    free (bounce);

    return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
    void
inode_deny_write (struct inode *inode) 
{
    inode->deny_write_cnt++;
    //printf("    INODE_DENY_WRITE: INODE %d DENY %d\n", inode, inode->deny_write_cnt);
    ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
    void
inode_allow_write (struct inode *inode) 
{
    ASSERT (inode->deny_write_cnt > 0);
    ASSERT (inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
    //printf("    INODE_ALLOW_WRITE: INODE %d DENY %d\n", inode, inode->deny_write_cnt);
}

/* Returns the length, in bytes, of INODE's data. */
    off_t
inode_length (const struct inode *inode)
{
    return inode->data.length;
}

    int 
inode_cnt (const struct inode *inode)
{
    return inode->deny_write_cnt;
}

static bool inode_allocate (size_t sectors, struct inode_disk *disk_inode)
{
    disk_inode->direct_idx = 0;
    disk_inode->indirect_idx = 0;
    disk_inode->double_indirect_idx = 0;

    int iter = 0;

    while (sectors > 0)
    {
	if (iter < DIRECT_PTR_NUM)
	{
	    if (free_map_allocate (1, &disk_inode->directory[iter]))
	    {
		sectors --;
		disk_inode->direct_idx++;
	    }
	    else
		goto error;
	}

	else if (iter < DIRECT_PTR_NUM + INDIRECT_PTR_NUM)
	{
	    if (free_map_allocate (1, &disk_inode->directory[iter]))
	    {	
		int idx = indirect_allocate (sectors, disk_inode->directory[iter]);
		if (idx == ERROR_ALLOC)
		    goto error;
		else
		{
		    sectors -= idx;
		    disk_inode->indirect_idx += idx;
		}
	    }
	}
	else if (iter < DIRECT_PTR_NUM + INDIRECT_PTR_NUM + DOUBLE_INDIRECT_PTR_NUM)
	{
	    if (free_map_allocate (1, &disk_inode->directory[iter]))
	    {
		int idx = double_indirect_allocate (sectors, disk_inode->directory[iter]);

		if (idx == ERROR_ALLOC)
		    goto error;
		else
		{
		    sectors -= idx;
		    disk_inode->double_indirect_idx += idx;
		}
	    
	    }
	}
	else{
	    PANIC ("Memory full!");
	}

	iter++;
    }

    return true;

error:
    /* dealloc all blocks */
    return false;
}

static int indirect_allocate (size_t sectors, disk_sector_t indirect_block)
{
    static char zeros[DISK_SECTOR_SIZE] = {0};
    disk_sector_t block_ctx [INDIRECT_BLOCK_SIZE] = {0}; 

    int iter = 0; 

    while (sectors > 0 && iter < INDIRECT_BLOCK_SIZE)
    {
	if (free_map_allocate (1, &block_ctx[iter]))
	{
	    sectors--;
	    disk_write (filesys_disk, block_ctx[iter], zeros);
	}
	else
	    goto error;

	iter++;
    }
    
    disk_write (filesys_disk, indirect_block, block_ctx);

    return iter;

error:
    /* dealloc indirect block */
    while (iter >= 0)
    {
	free_map_release (block_ctx[iter], 1);
	iter--;
    }

    return ERROR_ALLOC;
}

static int double_indirect_allocate (size_t sectors, disk_sector_t double_indirect_block)
{
    disk_sector_t block_ctx [INDIRECT_BLOCK_SIZE] = {0};

    int iter = 0, ret = 0; 
    size_t alloc_sectors;

    while (sectors > 0 && iter < INDIRECT_BLOCK_SIZE)
    {
	alloc_sectors = sectors < INDIRECT_BLOCK_SIZE ? sectors : INDIRECT_BLOCK_SIZE;
	
	if (free_map_allocate (1, &block_ctx[iter]))
	{
	    int idx = indirect_allocate (alloc_sectors, block_ctx[iter]);

	    if (idx == ERROR_ALLOC)
		goto error;
	    else
	    {
		sectors -= idx;
		ret += idx;
	    }
	}
	else 
	    goto error;

	iter++;
    }
    
    disk_write (filesys_disk, double_indirect_block, block_ctx);

    return ret;

error:
    /* dealloc indirect block */
    return ERROR_ALLOC;
}

static void inode_deallocate (struct inode_disk *disk_inode)
{
    off_t idx, idx_max;

    while (disk_inode->direct_idx > 0)
    {
	free_map_release (disk_inode->directory[disk_inode->direct_idx - 1], 1);
	disk_inode->direct_idx--;
    }

    while (disk_inode->indirect_idx > 0)
    {	
	idx = (disk_inode->indirect_idx - 1) / INDIRECT_BLOCK_SIZE;
	idx_max = (idx % INDIRECT_BLOCK_SIZE == 0) ? (idx > 0 ? INDIRECT_BLOCK_SIZE : 0) : idx % INDIRECT_BLOCK_SIZE;
	indirect_deallocate (disk_inode->directory[idx + DIRECT_PTR_NUM], idx_max);
	free_map_release (disk_inode->directory[idx + DIRECT_PTR_NUM], 1);
	disk_inode->indirect_idx -= idx_max;
    }

    if (disk_inode->double_indirect_idx > 0)
    {
	double_indirect_deallocate (disk_inode->directory[DIRECT_PTR_NUM + INDIRECT_PTR_NUM], (disk_inode->double_indirect_idx - 1) / INDIRECT_BLOCK_SIZE, disk_inode->double_indirect_idx);
    }
}

static void indirect_deallocate (disk_sector_t sector, size_t cnt)
{
    unsigned iter;
    disk_sector_t block[INDIRECT_BLOCK_SIZE];

    disk_read (filesys_disk, sector, block);

    for (iter = 0; iter < cnt; iter++)
	free_map_release (block[iter], 1);

    free_map_release (sector, 1);
}

static void double_indirect_deallocate (disk_sector_t sector, size_t cnt, size_t sectors)
{
    unsigned iter;
    size_t block_sectors;
    disk_sector_t block[INDIRECT_BLOCK_SIZE];

    disk_read (filesys_disk, sector, block);

    for (iter = 0; iter < cnt; iter++)
    {
	block_sectors = sectors < INDIRECT_BLOCK_SIZE ? sectors : INDIRECT_BLOCK_SIZE;
	indirect_deallocate (block[iter], block_sectors);
	sectors -= block_sectors;
    }

    free_map_release (sector, 1);
}

static off_t expand_file (struct inode *inode, off_t length)
{
    struct inode_disk *disk_inode = &inode->data;
    disk_sector_t sectors = bytes_to_sectors (length) - bytes_to_sectors (inode->data.length);
    disk_sector_t block[INDIRECT_BLOCK_SIZE] = {0};
    static char zeros[DISK_SECTOR_SIZE] = {0};

    while (sectors > 0)
    {
	/* expand using direct blocks */
	if (disk_inode->direct_idx < DIRECT_PTR_NUM)
	{
	    if (free_map_allocate (1, &disk_inode->directory[disk_inode->direct_idx]))
	    {
		disk_write (filesys_disk, disk_inode->directory[disk_inode->direct_idx], zeros);
		sectors--;
		disk_inode->direct_idx++;
	    }
	    else 
		goto error;
	}

	/* expand using indirect blocks */
	else if (disk_inode->indirect_idx < INDIRECT_PTR_NUM * INDIRECT_BLOCK_SIZE)
	{
	    off_t block_idx = (disk_inode->indirect_idx - 1) / INDIRECT_BLOCK_SIZE;
	    off_t inblock_idx = (disk_inode->indirect_idx - 1) % INDIRECT_BLOCK_SIZE;

	    if (inblock_idx == INDIRECT_BLOCK_SIZE - 1)
	    {
		if (free_map_allocate (1, &disk_inode->directory[block_idx + DIRECT_PTR_NUM + 1]))
		{
		    if (free_map_allocate (1, &block[0]))
		    {
			sectors--;
			disk_inode->indirect_idx++;
			disk_write (filesys_disk, block[0], zeros);
			disk_write (filesys_disk, disk_inode->directory[block_idx + DIRECT_PTR_NUM + 1], block);
			memset (block, 0, sizeof block);
		    }
		    else
			goto error;
		}
		else
		    goto error;
	    }
	    else
	    {
		disk_read (filesys_disk, disk_inode->directory[block_idx + DIRECT_PTR_NUM], block);
		
		if (free_map_allocate (1, &block[inblock_idx + 1]))
		{
		    sectors--;
		    disk_inode->indirect_idx++;
		    disk_write (filesys_disk, block[inblock_idx + 1], zeros);
		    disk_write (filesys_disk, disk_inode->directory[block_idx + DIRECT_PTR_NUM], block);
		    memset (block, 0, sizeof block);
		}
		else
		    goto error;
	    }
	}
	/* expand using double indirect blocks */
	else if (disk_inode->double_indirect_idx < INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_SIZE)
	{
	    
	}

	else
	{
    	    return length - sectors * DISK_SECTOR_SIZE;
	}
    }    

    return length;

error:
    PANIC ("Expand fail");
}

//static void expand_indirect (struct inode *inode, disk_sector_t sectors)
//{
//    static char zeros[DISK_SECTOR_SIZE];
//    disk_sector_t block[INDIRECT_BLOCK_SIZE];
//
//
//
//static void expand_double_indirect (struct inode *inode, disk_sector_t sectors)
//{
//    while (sectors > 0)
//    {
//	off_t block_idx = (disk_inode->double_indirect_idx - 1) / INDIRECT_BLOCK_SIZE;
//	off_t double_block_idx = block_idx / INDIRECT_BLOCK_SIZE;
//	off_t in_block_idx = (disk_inode->double_indirect_idx - 1) % INDIRECT_BLOCK_SIZE;
//
//
//
//    }
//
//
//}
//



//static 
//int32_t direct_allocate (size_t sectors, struct inode_disk *disk_inode)
//{
//    static char zeros[DISK_SECTOR_SIZE] = {0};
//    int iter; 
//
//    for (iter = 0 ; iter < DIRECT_PTR_NUM ; iter++)
//    {
//	if (sectors > 0)
//	{
//	    if (free_map_allocate (1, &disk_inode->directory[iter]))
//	    {
//		disk_write (filesys_disk, disk_inode->directory[iter], zeros); 
//		sectors--;
//	    }
//	    else 
//	    {
//		goto error;
//	    }
//	}
//	else
//	    return iter;
//   }	 
//
//    return iter;
//
//error:
//    /*dealloc direct block */
//    return ERROR_ALLOC;
//}

//int32_t indirect_allocate (size_t sectors, struct inode_disk *disk_inode)
//{
//    static char zeros[DISK_SECTOR_SIZE] = {0};
//    disk_sector_t block [DISK_SECTOR_SIZE/4] = {0}; 
//
//    int iter; 
//    int iter_indirect;
//    int block_num = 0;
//    size_t alloc_sectors;
//
//    for (iter = 0; iter < INDIRECT_PTR_NUM; iter++)
//    {
//	alloc_sectors = sectors >= (DISK_SECTOR_SIZE * DISK_SECTOR_SIZE / 4) ? 
//	    (DISK_SECTOR_SIZE * DISK_SECTOR_SIZE / 4) : sectors;
//	
//	if (alloc_sectors > 0)
//	{
//	    if (free_map_allocate (1, &disk_inode->directory[iter]))
//	    {
//		iter_indirect = 0;
//
//		while (alloc_sectors > 0)
//		{
//		    if (free_map_allocate (1, &block[iter_indirect]))
//		    {
//			block_num++;
//			iter_indrect++;
//			alloc_sectors -= DISK_SECTOR_SIZE;
//			sectors -= DISK_SECTOR_SIZE;
//			disk_write (filesys_disk, block[iter_indirect], zeros);
//
//			memset (block, 0, sizeof block);
//		    }
//		    else
//			goto error;
//		}
//
//		disk_write (filesys_disk, disk_inode->directory[iter], block);
//	    }
//	    else
//		goto error;
//	}
//	else
//	   return block_num;
//    }
//    return block_num;
//
//error:
//    /* dealloc indirect block */
//    return ERROR_ALLOC;
//}

