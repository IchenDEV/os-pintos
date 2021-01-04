#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block* fs_device;

static void do_format(void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void filesys_init(bool format) {
  fs_device = block_get_role(BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC("No file system device found, can't initialize file system.");
 // init_cache();
  inode_init();
  free_map_init();

  if (format)
    do_format();

  free_map_open();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void filesys_done(void) { free_map_close(); }

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool filesys_create(const char* name, off_t initial_size,bool is_dir) {
  block_sector_t inode_sector = 0;

  struct dir* dir = dir_get_from_path(name);
  char* file_name = path_final_name(name);

  bool success =
      (dir != NULL && free_map_allocate(1, &inode_sector) &&
       inode_create(inode_sector, initial_size,is_dir) && dir_add(dir, file_name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release(inode_sector, 1);
  dir_close(dir);
 free(file_name);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file* filesys_open(const char* name) {
  if (strlen(name) == 0)
    return NULL;

  struct dir* dir = dir_get_from_path(name);
  char* file_name = path_final_name(name);
  struct inode* inode = NULL;

  if (dir != NULL) {
    if (strcmp(file_name, "..") == 0) {
      inode = dir_parent_inode(dir);
      if (!inode) {
        free(file_name);
        return NULL;
      }
    } else if ((dir_is_root(dir) && strlen(file_name) == 0) || strcmp(file_name, ".") == 0) {
      free(file_name);
      return (struct file*)dir;
    } else
      dir_lookup(dir, file_name, &inode);
  }

  free(file_name);
  dir_close(dir);

  if (!inode)
    return NULL;

  if (inode_is_dir(inode))
    return (struct file*)dir_open(inode);

  return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool filesys_remove(const char* name) {
  struct dir* dir = dir_get_from_path(name);
  char* file_name = path_final_name(name);
  bool success = dir != NULL && dir_remove(dir, file_name);
  dir_close(dir);
  free(file_name);
  return success;
}

// ADDED
bool filesys_chdir(const char* path) {
  struct dir* dir = dir_get_from_path(path);
  char* name = path_final_name(path);
  struct inode* inode = NULL;

  if (dir == NULL) {
    free(name);
    return false;
  }
  // special case: go to parent dir
  else if (strcmp(name, "..") == 0) {
    inode = dir_parent_inode(dir);
    if (inode == NULL) {
      free(name);
      return false;
    }
  }
  // special case: current dir
  else if (strcmp(name, ".") == 0 || (strlen(name) == 0 && dir_is_root(dir))) {
    thread_current()->dir = dir;
    free(name);
    return true;
  } else
    dir_lookup(dir, name, &inode);

  dir_close(dir);

  // now open up target dir
  dir = dir_open(inode);

  if (dir == NULL) {
    free(name);
    return false;
  } else {
    dir_close(thread_current()->dir);
    thread_current()->dir = dir;
    free(name);
    return true;
  }
}

/* Formats the file system. */
static void do_format(void) {
  printf("Formatting file system...");
  free_map_create();
  if (!dir_create(ROOT_DIR_SECTOR, 16))
    PANIC("root directory creation failed");
  free_map_close();
  printf("done.\n");
}

char* path_final_name(const char* path_name) {
  int length = strlen(path_name);
  char path[length + 1];
  memcpy(path, path_name, length + 1);

  char *cur, *ptr, *prev = "";
  for (cur = strtok_r(path, "/", &ptr); cur != NULL; cur = strtok_r(NULL, "/", &ptr))
    prev = cur;

  char* name = malloc(strlen(prev) + 1);
  memcpy(name, prev, strlen(prev) + 1);
  return name;
}

struct dir* dir_get_from_path(const char* path_name) {
  int length = strlen(path_name);
  char path[length + 1];
  memcpy(path, path_name, length + 1);

  struct dir* dir;
  if (path[0] == '/' || !thread_current()->dir)
    dir = dir_open_root();
  else
    dir = dir_reopen(thread_current()->dir);

  char *cur, *ptr, *prev;
  prev = strtok_r(path, "/", &ptr);
  for (cur = strtok_r(NULL, "/", &ptr); cur != NULL; prev = cur, cur = strtok_r(NULL, "/", &ptr)) {
    struct inode* inode;
    if (strcmp(prev, ".") == 0)
      continue;
    else if (strcmp(prev, "..") == 0) {
      inode = dir_parent_inode(dir);
      if (inode == NULL)
        return NULL;
    } else if (dir_lookup(dir, prev, &inode) == false)
      return NULL;

    if (inode_is_dir(inode)) {
      dir_close(dir);
      dir = dir_open(inode);
    } else
      inode_close(inode);
  }

  return dir;
}