#include "filesys/cache.h"
#include "filesys/filesys.h"
#include <string.h>

static struct cache cache_arr[BUF_CACHE_SIZE];

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
    cache_id idx = find_cache (pos);

    lock_acquire (&cache_lock);

    if (idx == -1)
    {
	idx = evict_cache ();
	cache_arr[idx].pos = pos;
	cache_arr[idx].dirty = false;
	disk_read (filesys_disk, pos, &cache_arr[idx].data);
    }
    
    memcpy (buffer, &cache_arr[idx].data + ofs, size);
    cache_arr[idx].accessed = true;
    lock_release (&cache_lock);
}

void write_cache (disk_sector_t pos, void *buffer, off_t size, off_t ofs)
{
    cache_id idx = find_cache (pos);

    lock_acquire (&cache_lock);

    if (idx == -1)
    {
	idx = evict_cache ();
	cache_arr[idx].pos = pos;
	cache_arr[idx].dirty = false;
	
	if (ofs > 0 || size < DISK_SECTOR_SIZE - ofs)
	    disk_read (filesys_disk, pos, &cache_arr[idx].data);
    }
    
    memcpy (cache_arr[idx].data + ofs, buffer, size); 
    cache_arr[idx].dirty = true;
    lock_release (&cache_lock);
}

static cache_id evict_cache (void)
{
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
	    memset (&cache_arr[i], 0, sizeof (struct cache));
	    return i;
	}
    }


    for (i = 0; i < BUF_CACHE_SIZE; i++)
    {
	if (cache_arr[i].dirty && cache_arr[i].accessed)
	{
	    memset (&cache_arr[i], 0, sizeof (struct cache));
	    return i;
	}
    }
}

    
