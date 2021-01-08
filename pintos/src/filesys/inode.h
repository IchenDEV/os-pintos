#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H
#include <list.h>
#include <stdbool.h>
#include "threads/synch.h"
#include "filesys/off_t.h"
#include "devices/block.h"
/* Block Sector Counts */
#define DIRECT_BLOCK_COUNT 123
#define INDIRECT_BLOCK_COUNT 128

struct bitmap;
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
  block_sector_t direct_blocks[DIRECT_BLOCK_COUNT];
  block_sector_t indirect_block;
  block_sector_t doubly_indirect_block;

  bool is_dir;    /* Indicator of directory file */
  off_t length;   /* File size in bytes. */
  unsigned magic; /* Magic number. */
};
/* In-memory inode. */
struct inode {
  struct list_elem elem;  /* Element in inode list. */
  block_sector_t sector;  /* Sector number of disk location. */
  int open_cnt;           /* Number of openers. */
  struct lock inode_lock; /* Inode lock. */
  bool removed;           /* True if deleted, false otherwise. */
  int deny_write_cnt;     /* 0: writes ok, >0: deny writes. */
};

void inode_init(void);
bool inode_create(block_sector_t, off_t, bool);
struct inode* inode_open(block_sector_t);
struct inode* inode_reopen(struct inode*);
block_sector_t inode_get_inumber(const struct inode*);
void inode_close(struct inode*);
void inode_remove(struct inode*);
off_t inode_read_at(struct inode*, void*, off_t size, off_t offset);
off_t inode_write_at(struct inode*, const void*, off_t size, off_t offset);
void inode_deny_write(struct inode*);
void inode_allow_write(struct inode*);
struct inode_disk* get_inode_disk(const struct inode*);
off_t inode_length(const struct inode*);
bool inode_is_dir(const struct inode*);
bool inode_is_removed(const struct inode*);

void inode_acquire_lock(struct inode* inode);
void inode_release_lock(struct inode* inode);

#endif /* filesys/inode.h */