#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool dir_create(block_sector_t sector, size_t entry_cnt) {
  bool success = true;

  success = inode_create(sector, entry_cnt * sizeof(struct dir_entry), /*is_dir*/ true);
  if (!success)
    return false;

  // The first (offset 0) dir entry is for parent directory; do self-referencing
  // Actual parent directory will be set on execution of dir_add()
  struct dir* dir = dir_open(inode_open(sector));
  ASSERT(dir != NULL);
  struct dir_entry e;
  e.inode_sector = sector;
  if (inode_write_at(dir->inode, &e, sizeof e, 0) != sizeof e) {
    success = false;
  }
  return success;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir* dir_open(struct inode* inode) {
  struct dir* dir = calloc(1, sizeof *dir);
  struct dir_entry e;
  if (inode != NULL && dir != NULL) {
    dir->inode = inode;
    dir->pos = sizeof e;
    return dir;
  } else {
    inode_close(inode);
    free(dir);
    return NULL;
  }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir* dir_open_root(void) {
  return dir_open(inode_open(ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir* dir_reopen(struct dir* dir) {
  return dir_open(inode_reopen(dir->inode));
}

/* Destroys DIR and frees associated resources. */
void dir_close(struct dir* dir) {
  if (dir != NULL) {
    inode_close(dir->inode);
    free(dir);
  }
}

/* Returns the inode encapsulated by DIR. */
struct inode* dir_get_inode(struct dir* dir) {
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool lookup(const struct dir* dir, const char* name, struct dir_entry* ep, off_t* ofsp) {
  struct dir_entry e;
  size_t ofs;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e)
    if (e.in_use && !strcmp(name, e.name)) {
      if (ep != NULL)
        *ep = e;
      if (ofsp != NULL)
        *ofsp = ofs;
      return true;
    }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool dir_lookup(const struct dir* dir, const char* name, struct inode** inode) {
  struct dir_entry e;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  if (lookup(dir, name, &e, NULL))
    *inode = inode_open(e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool dir_add(struct dir* dir, const char* name, block_sector_t inode_sector, bool is_dir) {
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen(name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup(dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e)
    if (!e.in_use)
      break;
  /* Handle the case when a directory is added in current directory.
     Open the target directory and create a reference to the current directory
     as its parent directory. This should be done in the offset 0 of the child directory
     as it's the convention for finding its parent. */

  if (is_dir) {
    struct dir* child = dir_open(inode_open(inode_sector));
    if (!child)
      goto done;
    struct dir_entry parent;
    parent.inode_sector = inode_get_inumber(dir_get_inode(dir));
    parent.in_use = true;
    strlcpy(parent.name, "..", NAME_MAX + 1);
    size_t rc = inode_write_at(child->inode, &parent, sizeof parent, 0);
    dir_close(child);
    if (rc != sizeof(parent))
      goto done;
  }

  /* Write slot. */
  e.in_use = true;
  e.inode_sector = inode_sector;
  strlcpy(e.name, name, sizeof e.name);
  int wcs = inode_write_at(dir->inode, &e, sizeof e, ofs);
  //printf("wcs: %d  %d\n",wcs,ofs);
  success = wcs == sizeof e;
done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool dir_remove(struct dir* dir, const char* name) {
  struct dir_entry e;
  struct inode* inode = NULL;
  bool success = false;
  off_t ofs;
  ASSERT(dir != NULL);
  ASSERT(name != NULL);
  /* Find directory entry. */
  if (!lookup(dir, name, &e, &ofs))
    goto done;
  /* Open inode. */
  inode = inode_open(e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Prevent removing non-empty directory. */
  if (inode_is_dir(inode)) {
    // target : the directory to be removed. (dir : the base directory)
    struct dir* target = dir_open(inode);
    bool is_empty = dir_is_empty(target);
    dir_close(target);
    if (!is_empty)
      goto done; // can't delete
  }
  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at(dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove(inode);
  success = true;
done:
  if (inode != NULL)
    inode_close(inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool dir_readdir(struct dir* dir, char name[NAME_MAX + 1]) {
  struct dir_entry e;
  if (dir->pos == 0) {
    dir->pos = sizeof e;
  }
  for (; /* 0-pos is for parent directory */
       inode_read_at(dir->inode, &e, sizeof e, dir->pos) == sizeof e;) {
    dir->pos += sizeof e;
    if (e.in_use) {
      strlcpy(name, e.name, NAME_MAX + 1);
      return true;
    }
  }
  return false;
}

bool dir_is_root(struct dir* dir) {
  if (dir != NULL && inode_get_inumber(dir_get_inode(dir)) == ROOT_DIR_SECTOR)
    return true;
  else
    return false;
}
bool dir_is_same(struct dir* dir1, struct dir* dir2) {
  if (dir1 != NULL && dir2 != NULL &&
      inode_get_inumber(dir_get_inode(dir1)) == inode_get_inumber(dir_get_inode(dir2)))
    return true;
  else
    return false;
}

bool dir_is_empty(struct dir* dir) {
  struct dir_entry e;
  off_t ofs;

  for (ofs = sizeof e; /* 0-pos is for parent directory */
       inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e) {
    if (e.in_use)
      return false;
  }
  return true;
}

struct dir* dir_parent(struct dir* dir) {
  struct dir_entry e;
  off_t ofs;

  for (ofs = 0; /* 0-pos is for parent directory */
       inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e) {
    return dir_open(inode_open(e.inode_sector));
  }
  return NULL;
}
