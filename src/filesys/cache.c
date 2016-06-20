#include "filesys/cache.h"
#include "filesys/filesys.h"
#include <string.h>
#include <debug.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include <stdio.h>
//////more things to implement
//////1. writer into the file perodically
//////- using timer_sleep and thread
//////2. read ahead
//////- using thread

#define BUF_WRITE_TICKS 100

struct cache{
    disk_sector_t pos;
    char data[DISK_SECTOR_SIZE];

    bool dirty;
    bool accessed;
    struct lock cache_lock;
};

struct read_ahead{
    disk_sector_t pos;
    struct list_elem read_ahead_elem;
};

static struct cache cache_arr[BUF_CACHE_SIZE];
static int cache_num = 0; 
static struct lock cache_lock;
static struct lock read_ahead_lock;
static disk_sector_t disk_length;
static struct list read_ahead_list;
static struct condition read_ahead_cond;


static cache_id evict_cache (void);
static void cache_clear (cache_id);
static void periodic_write (void* aux UNUSED);

void init_cache (void)
{
    int iter;

    lock_init (&cache_lock); 
    lock_init (&read_ahead_lock);
    list_init (&read_ahead_list);
    cond_init (&read_ahead_cond);
    for (iter = 0; iter < BUF_CACHE_SIZE; iter++)
	lock_init (&cache_arr[iter].cache_lock);

    thread_create ("read_ahead_thread", PRI_DEFAULT, thread_read_ahead, NULL); 
    thread_create ("periodic_write_thread", PRI_DEFAULT, periodic_write, NULL);
    disk_length = disk_size (disk_get (0, 1));
}

cache_id find_cache (disk_sector_t pos)
{
    int i;

    //lock_acquire (&cache_lock);
    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (cache_arr[i].pos == pos)
	{
	    //lock_release (&cache_lock);
	    return i;
	}
    }
    //lock_release (&cache_lock);
    return -1;
}

void read_cache (disk_sector_t pos, void *buffer, off_t size, off_t ofs)
{
    /* read head */
    lock_acquire (&read_ahead_lock);

    struct read_ahead *ra = malloc (sizeof (struct read_ahead));
    ra->pos = pos + 1;
    if (ra->pos < disk_length)
        list_push_back (&read_ahead_list, &ra->read_ahead_elem);
    else 
	free (ra);

    cond_signal (&read_ahead_cond, &read_ahead_lock);
    
    lock_release (&read_ahead_lock);
      
    /* read cache start */
    lock_acquire (&cache_lock);
    cache_id idx = find_cache (pos);

    if (idx == -1)
    {
	if (cache_num < BUF_CACHE_SIZE)
	{
	    //lock_acquire (&cache_lock);
	    cache_arr[cache_num].pos = pos;
	    cache_arr[cache_num].dirty = false;
	    disk_read (filesys_disk, pos, cache_arr[cache_num].data);

	    memcpy (buffer, cache_arr[cache_num].data + ofs, size);
	    cache_arr[cache_num].accessed = true;
	    cache_num ++;
	    lock_release (&cache_lock);

	}
	else
	{
	    //lock_acquire (&cache_lock);
	    idx = evict_cache ();
	    cache_arr[idx].pos = pos;
	    cache_arr[idx].dirty = false;
	    disk_read (filesys_disk, pos, cache_arr[idx].data);

	    memcpy (buffer, cache_arr[idx].data + ofs, size);
	    cache_arr[idx].accessed = true;
	    lock_release (&cache_lock);

	}
    }
    else
    {	
	lock_release (&cache_lock);
	//lock_acquire (&cache_lock);
	lock_acquire (&cache_arr[idx].cache_lock);
	memcpy (buffer, cache_arr[idx].data + ofs, size);
	cache_arr[idx].accessed = true;
	lock_release (&cache_arr[idx].cache_lock);
	//lock_release (&cache_lock);
    }
    //printf ("READ_CACHE END\n");
}

void write_cache (disk_sector_t pos, void *buffer, off_t size, off_t ofs)
{
    lock_acquire (&cache_lock);
    cache_id idx = find_cache (pos);

    if (idx == -1)
    {
	if (cache_num < BUF_CACHE_SIZE)
	{
	    //lock_acquire (&cache_lock);
	    cache_arr[cache_num].pos = pos;
	    cache_arr[cache_num].accessed = false;

	    if (ofs > 0 || size < DISK_SECTOR_SIZE - ofs)
		disk_read (filesys_disk, pos, cache_arr[cache_num].data);
	    
	    memcpy (cache_arr[cache_num].data + ofs, buffer, size); 
	    cache_arr[cache_num].dirty = true;
	    cache_num ++;
	    lock_release (&cache_lock);

	}
	else
	{
	    //lock_acquire (&cache_lock);
	    idx = evict_cache ();
	    cache_arr[idx].pos = pos;
	    cache_arr[idx].dirty = false;

	    if (ofs > 0 || size < DISK_SECTOR_SIZE - ofs)
		disk_read (filesys_disk, pos, cache_arr[idx].data);
	    
	    memcpy (cache_arr[idx].data + ofs, buffer, size); 
	    cache_arr[idx].dirty = true;
	    lock_release (&cache_lock);
	}

    }
    else
    {
	lock_release (&cache_lock);
	//lock_acquire (&cache_lock);
	lock_acquire (&cache_arr[idx].cache_lock);
	memcpy (cache_arr[idx].data + ofs, buffer, size); 
	cache_arr[idx].dirty = true;
	lock_release (&cache_arr[idx].cache_lock);
	//lock_release (&cache_lock);
    }
}

static cache_id evict_cache (void)
{
    ASSERT (cache_num == BUF_CACHE_SIZE);

    int i;

    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (!cache_arr[i].dirty && !cache_arr[i].accessed)
	{
	    lock_acquire (&cache_arr[i].cache_lock);
	    //memset (&cache_arr[i], 0, sizeof (struct cache) - sizeof (struct lock));
	    cache_clear (i);
	    lock_release (&cache_arr[i].cache_lock);
	    return i;
	}
    }

    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (!cache_arr[i].dirty && cache_arr[i].accessed)
	{
	    lock_acquire (&cache_arr[i].cache_lock);
	    //memset (&cache_arr[i], 0, sizeof (struct cache));
	    cache_clear (i);
	    lock_release (&cache_arr[i].cache_lock);
	    return i;
	}
    }


    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (cache_arr[i].dirty && !cache_arr[i].accessed)
	{
	    lock_acquire (&cache_arr[i].cache_lock);
	    disk_write (filesys_disk, cache_arr[i].pos, cache_arr[i].data);
	    //memset (&cache_arr[i], 0, sizeof (struct cache));
	    cache_clear (i);
	    lock_release (&cache_arr[i].cache_lock);
	    return i;
	}
    }


    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (cache_arr[i].dirty && cache_arr[i].accessed)
	{
	    lock_acquire (&cache_arr[i].cache_lock);
	    disk_write (filesys_disk, cache_arr[i].pos, cache_arr[i].data);
	    //memset (&cache_arr[i], 0, sizeof (struct cache));
	    cache_clear (i);
	    lock_release (&cache_arr[i].cache_lock);
	    return i;
	}
    }

    return -1;
}

static void cache_clear (cache_id idx)
{
    memset (&cache_arr[idx].data, 0, sizeof cache_arr[idx].data);
    cache_arr[idx].pos = 0;
    cache_arr[idx].accessed = false;
    cache_arr[idx].dirty = false;
}

void cache_to_disk (void)
{
    int i;

    lock_acquire (&cache_lock);

    for (i = 0; i < cache_num ; i++)
    {
	if (cache_arr[i].dirty)
	    disk_write (filesys_disk, cache_arr[i].pos, cache_arr[i].data);
    }

    lock_release (&cache_lock);
}


void thread_read_ahead (void *aux UNUSED)
{
    while (true)
    {
	lock_acquire (&read_ahead_lock);
	while (list_empty (&read_ahead_list))
	    cond_wait (&read_ahead_cond, &read_ahead_lock);

	/* critical section - read ahead */
	struct list_elem *elem = list_pop_front (&read_ahead_list);
	struct read_ahead *ra = list_entry (elem, struct read_ahead, read_ahead_elem);

	disk_sector_t pos = ra->pos;

	lock_acquire (&cache_lock);	
	cache_id idx = find_cache (pos);

	if (idx == -1)
	{
	    if (cache_num < BUF_CACHE_SIZE)
	    {
		cache_arr[cache_num].pos = pos;
		cache_arr[cache_num].dirty = false;
		cache_arr[cache_num].accessed = true;
		disk_read (filesys_disk, pos, cache_arr[cache_num].data);
		cache_num ++;
		
	    }
	    else
	    {
		idx = evict_cache ();
		cache_arr[idx].pos = pos;
		cache_arr[idx].dirty = false;
		cache_arr[idx].accessed = true;
		disk_read (filesys_disk, pos, cache_arr[idx].data);	
	    }
	    
	}
	
	lock_release (&cache_lock);

	free (ra);
	lock_release (&read_ahead_lock);
    }
}

static void periodic_write (void *aux UNUSED)
{
    while (true)
    {
	timer_sleep (BUF_WRITE_TICKS);
	cache_to_disk ();
    }
}

