#include "filesys/cache.h"
#include "filesys/filesys.h"
#include <string.h>
#include <debug.h>
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

static struct cache cache_arr[BUF_CACHE_SIZE];
static int cache_num = 0; 
static struct lock cache_lock;

static cache_id evict_cache (void);
static void cache_clear (cache_id);

void init_cache (void)
{
    int iter;

    lock_init (&cache_lock);
    
    for (iter = 0; iter < BUF_CACHE_SIZE; iter++)
	lock_init (&cache_arr[iter].cache_lock);
}

cache_id find_cache (disk_sector_t pos)
{
    int i;

    lock_acquire (&cache_lock);
    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (cache_arr[i].pos == pos)
	{
	    lock_release (&cache_lock);
	    return i;
	}
    }
    lock_release (&cache_lock);
    return -1;
}

void read_cache (disk_sector_t pos, void *buffer, off_t size, off_t ofs)
{
    //printf ("READ_CACHE START\n");
    cache_id idx = find_cache (pos);
    //cache x
    if (idx == -1)
    {
	if (cache_num < BUF_CACHE_SIZE)
	{
	    lock_acquire (&cache_lock);
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
	    lock_acquire (&cache_lock);
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
    {	//lock_acquire (&cache_lock);
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
    cache_id idx = find_cache (pos);

    if (idx == -1)
    {
	if (cache_num < BUF_CACHE_SIZE)
	{
	    lock_acquire (&cache_lock);
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
	    lock_acquire (&cache_lock);
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

