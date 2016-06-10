#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/disk.h"
#include <stdbool.h>
#include "threads/synch.h"
#include "filesys/off_t.h"

#define BUF_CACHE_SIZE 64 
typedef int cache_id;

void init_cache (void);
cache_id find_cache (disk_sector_t);
void read_cache (disk_sector_t, void*, off_t, off_t);
void write_cache (disk_sector_t, void*, off_t, off_t); 
void cache_to_disk (void);




#endif
