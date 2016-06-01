#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/disk.h"
#include <stdbool.h>
#include "threads/synch.h"
#include "filesys/off_t.h"

#define BUF_CACHE_SIZE 64 
typedef int cache_id;

extern struct lock cache_lock;

struct cache{
    disk_sector_t pos;
    char data [DISK_SECTOR_SIZE];

    bool dirty;
    bool accessed;
};

void init_cache (void);
cache_id find_cache (disk_sector_t);
void read_cache (disk_sector_t, void*, off_t, off_t);
void write_cache (disk_sector_t, void*, off_t, off_t); 





#endif
