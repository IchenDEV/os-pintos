#include <debug.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

#define CACHE_NUM_ENTRIES 64
#define CACHE_NUM_CHANCES 1

/* Cache block, each block can hold BLOCK_SECTOR_SIZE bytes of data. */
struct cache_block {
  struct lock cache_block_lock;
  block_sector_t disk_sector_index;
  uint8_t data[BLOCK_SECTOR_SIZE];

  bool valid;
  bool dirty;
  size_t chances;
};

/* Array of cache entries. */
static struct cache_block cache[CACHE_NUM_ENTRIES];

/* A lock for updating cache entries. */
static struct lock cache_update_lock;

/* Used to prevent flush on uninitialized cache if shutdown occurs before cache init. */
static bool cache_initialized = false;

/* Initialize the cache. */
void cache_init(void) {
  lock_init(&cache_update_lock);

  /* Initialize each cache block. */
  for (int i = 0; i < CACHE_NUM_ENTRIES; i++) {
    cache[i].valid = false;
    lock_init(&cache[i].cache_block_lock);
  }
  cache_initialized = true;
}

/* Writes the block at index to disk. */
static void cache_flush_block_index(struct block* fs_device, int index) {
  block_write(fs_device, cache[index].disk_sector_index, cache[index].data);
  cache[index].dirty = false;
}

/* Write entire cache to disk. */
void cache_flush(struct block* fs_device) {
  ASSERT(fs_device != NULL);
  /* Cache was not initialized and contains garbage so don't flush. */
  if (!cache_initialized)
    return;

  /* Write each cache block to disk */
  for (int i = 0; i < CACHE_NUM_ENTRIES; i++) {
    lock_acquire(&cache[i].cache_block_lock);
    if (cache[i].valid && cache[i].dirty)
      cache_flush_block_index(fs_device, i);
    lock_release(&cache[i].cache_block_lock);
  }
}

/* Invalidate the entire cache by invalidating all of its entries. */
void cache_invalidate(struct block* fs_device) {
  /* Cache was not initialized and contains garbage so it's already invalid. */
  if (!cache_initialized)
    return;

  lock_acquire(&cache_update_lock);
  /* Invalidate each cache entry. */
  for (int i = 0; i < CACHE_NUM_ENTRIES; i++) {
    lock_acquire(&cache[i].cache_block_lock);
    if (cache[i].valid && cache[i].dirty)
      cache_flush_block_index(fs_device, i);
    cache[i].valid = false;
    lock_release(&cache[i].cache_block_lock);
  }
  lock_release(&cache_update_lock);
}
/* Find a cache entry to evict */
static int cache_evict(struct block* fs_device, block_sector_t sector_index) {
  static size_t clock_position = 0;
  lock_acquire(&cache_update_lock);

  /* Check to make sure sector_index is not already in the cache. */
  int i;
  for (i = 0; i < CACHE_NUM_ENTRIES; i++) {
    lock_acquire(&cache[i].cache_block_lock);
    if (cache[i].valid && (cache[i].disk_sector_index == sector_index)) {
      lock_release(&cache_update_lock);
      return (-i) - 1;
    }
    lock_release(&cache[i].cache_block_lock);
  }

  /*  clock algorithm */
  while (true) {
    i = clock_position;
    clock_position++;
    clock_position %= CACHE_NUM_ENTRIES;

    lock_acquire(&cache[i].cache_block_lock);

    if (!cache[i].valid) {
      lock_release(&cache_update_lock);
      return i;
    }

    if (cache[i].chances == 0)
      break;

    cache[i].chances--;
    lock_release(&cache[i].cache_block_lock);
  }
  lock_release(&cache_update_lock);
  /* Write dirty block back to disk. */
  if (cache[i].dirty)
    cache_flush_block_index(fs_device, i);
  cache[i].valid = false;
  return i;
}

/* Replace the cache  sector_index. */
static void cache_replace(struct block* fs_device, int index, block_sector_t sector_index,
                          bool is_whole_block_write) {
  if (!is_whole_block_write)
    block_read(fs_device, sector_index, cache[index].data);

  cache[index].valid = true;
  cache[index].dirty = false;
  cache[index].disk_sector_index = sector_index;
  cache[index].chances = CACHE_NUM_CHANCES;
}

/* sector_index. */
static int cache_get_block_index(struct block* fs_device, block_sector_t sector_index,
                                 bool is_whole_block_write) {
  /* Check if sector_index is in cache. */
  int i;
  for (i = 0; i < CACHE_NUM_ENTRIES; i++) {
    lock_acquire(&cache[i].cache_block_lock);
    if (cache[i].valid && (cache[i].disk_sector_index == sector_index))
      break;
    lock_release(&cache[i].cache_block_lock);
  }

  /* Evict if sector_index is not in cache. */
  if (i == CACHE_NUM_ENTRIES) {
    i = cache_evict(fs_device, sector_index);
    if (i >= 0)
      cache_replace(fs_device, i, sector_index, is_whole_block_write);
    else
      i = -(i + 1);
  }
  return i;
}

/* Read . */
void cache_read(struct block* fs_device, block_sector_t sector_index, void* destination,
                off_t offset, int chunk_size) {
  ASSERT(fs_device != NULL);
  ASSERT(cache_initialized == true);

  /* cache_get_block_index () acquires cache_block_lock at index i. */
  int i = cache_get_block_index(fs_device, sector_index, false);
  ASSERT(cache[i].valid == true);

  memcpy(destination, cache[i].data + offset, chunk_size);
  cache[i].chances = CACHE_NUM_CHANCES;
  lock_release(&cache[i].cache_block_lock);
}

/* Write . */
void cache_write(struct block* fs_device, block_sector_t sector_index, void* source, off_t offset,
                 int chunk_size) {
  ASSERT(fs_device != NULL);
  ASSERT(cache_initialized == true);
  int i;
  if (chunk_size == BLOCK_SECTOR_SIZE)
    i = cache_get_block_index(fs_device, sector_index, true);
  else
    i = cache_get_block_index(fs_device, sector_index, false);
  ASSERT(cache[i].valid == true);

  memcpy(cache[i].data + offset, source, chunk_size);
  cache[i].dirty = true;
  cache[i].chances = CACHE_NUM_CHANCES;
  lock_release(&cache[i].cache_block_lock);
}