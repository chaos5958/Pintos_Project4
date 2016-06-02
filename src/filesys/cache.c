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

static struct cache cache_arr[BUF_CACHE_SIZE];
static int cache_num = 0; 

struct lock cache_lock;

static cache_id evict_cache (void);

void init_cache (void)
{
    lock_init (&cache_lock);
    memset (cache_arr, 0, sizeof (cache_arr));
}

cache_id find_cache (disk_sector_t pos)
{
    int i;

    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (cache_arr[i].pos == pos)
	    return i;
    }

    return -1;
}

void read_cache (disk_sector_t pos, void *buffer, off_t size, off_t ofs)
{
    lock_acquire (&cache_lock);

    cache_id idx = find_cache (pos);
    //cache x
    if (idx == -1)
    {
	if (cache_num < BUF_CACHE_SIZE)
	{
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
	memcpy (buffer, cache_arr[idx].data + ofs, size);
	cache_arr[idx].accessed = true;
	lock_release (&cache_lock);
    }
}

void write_cache (disk_sector_t pos, void *buffer, off_t size, off_t ofs)
{
    lock_acquire (&cache_lock);

    cache_id idx = find_cache (pos);

    if (idx == -1)
    {
	if (cache_num < BUF_CACHE_SIZE)
	{
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
	memcpy (cache_arr[idx].data + ofs, buffer, size); 
	cache_arr[idx].dirty = true;
	lock_release (&cache_lock);
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
	    memset (&cache_arr[i], 0, sizeof (struct cache));
	    return i;
	}
    }

    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (!cache_arr[i].dirty && cache_arr[i].accessed)
	{
	    memset (&cache_arr[i], 0, sizeof (struct cache));
	    return i;
	}
    }


    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (cache_arr[i].dirty && !cache_arr[i].accessed)
	{
	    disk_write (filesys_disk, cache_arr[i].pos, cache_arr[i].data);
	    memset (&cache_arr[i], 0, sizeof (struct cache));
	    return i;
	}
    }


    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (cache_arr[i].dirty && cache_arr[i].accessed)
	{
	    disk_write (filesys_disk, cache_arr[i].pos, cache_arr[i].data);
	    memset (&cache_arr[i], 0, sizeof (struct cache));
	    return i;
	}
    }
}

    
