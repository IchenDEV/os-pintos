#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block* fs_device;

static void do_format(void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void filesys_init(bool format) {
  fs_device = block_get_role(BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC("No file system device found, can't initialize file system.");

  inode_init();
  cache_init();
  free_map_init();

  if (format)
    do_format();
  free_map_open();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void filesys_done(void) {
  free_map_close();
  cache_flush(fs_device);
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool filesys_create(const char* name, off_t initial_size, bool is_dir) {
  block_sector_t inode_sector = 0;

  char directory[strlen(name) + 1];
  char filename[NAME_MAX + 1];
  directory[0] = '\0';
  filename[0] = '\0';

  split_directory_and_filename(name, directory, filename);
  struct dir* dir = dir_get_from_path(directory);

  bool success = false;
  if (is_dir) {
    success = (dir != NULL && free_map_allocate(1, &inode_sector) && dir_create(inode_sector, 1) &&
               dir_add(dir, filename, inode_sector, is_dir));
  } else {
    success = dir != NULL && free_map_allocate(1, &inode_sector);
    success = success && inode_create(inode_sector, initial_size, is_dir);
    success = success && dir_add(dir, filename, inode_sector, is_dir);
  }

  if (!success && inode_sector != 0)
    free_map_release(inode_sector, 1);
  dir_close(dir);

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
  char directory[strlen(name) + 1];
  char filename[NAME_MAX + 1];
  directory[0] = '\0';
  filename[0] = '\0';

  split_directory_and_filename(name, directory, filename);
  struct dir* dir = dir_get_from_path(directory);
  struct inode* inode = NULL;
  if (dir == NULL)
    return NULL;

  if (strlen(filename) == 0 || strcmp(filename, ".") == 0)
    inode = dir_get_inode(dir);
  else {
    dir_lookup(dir, filename, &inode);
    dir_close(dir);
  }

  if (inode == NULL || inode_is_removed(inode))
    return NULL;
  return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool filesys_remove(const char* name) {

  char directory[strlen(name) + 1];
  char filename[NAME_MAX + 1];
  directory[0] = '\0';
  filename[0] = '\0';

  bool split_success = split_directory_and_filename(name, directory, filename);
  struct dir* dir = dir_get_from_path(directory);
  struct thread* curr_thread = thread_current();
  struct dir* dirc = curr_thread->dir;
  bool is_parent = false;

  while (!dir_is_root(dirc) && dirc != NULL && strlen(filename) == 0) {
    dirc = dir_parent(dirc);
    bool same = dir_is_same(dirc, dir);
    if (same) {
      is_parent = true;
      break;
    }
  }
  if (dir_is_root(dir) && strlen(filename) == 0) {
    is_parent = true;
  }

  if (!is_parent) {

    bool success = split_success && (dir != NULL) && dir_remove(dir, filename);
    if (success)
      dir_close(dir);
    return success;
  } else {
    return false;
  }
}

bool filesys_chdir(const char* name) {
  struct dir* dir = dir_get_from_path(name);
  struct inode* inode = NULL;
  if (dir == NULL) {
    return false;
  } else {
    dir_close(thread_current()->dir);
    thread_current()->dir = dir;

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

/* Given a full PATH, extract the DIRECTORY and FILENAME into the provided pointers. */
bool split_directory_and_filename(const char* path, char* directory, char* filename) {
  if (strlen(path) == 0)
    return false;

  if (path[0] == '/')
    *directory++ = '/';

  int status;
  char token[NAME_MAX + 1], prev_token[NAME_MAX + 1];
  token[0] = '\0';
  prev_token[0] = '\0';

  while ((status = get_next_part(token, &path)) != 0) {
    if (status == -1)
      return false;

    int prev_length = strlen(prev_token);
    if (prev_length > 0) {
      memcpy(directory, prev_token, sizeof(char) * prev_length);
      directory[prev_length] = '/';
      directory += prev_length + 1;
    }
    memcpy(prev_token, token, sizeof(char) * strlen(token));
    prev_token[strlen(token)] = '\0';
  }

  *directory = '\0';
  memcpy(filename, token, sizeof(char) * (strlen(token) + 1));
  return true;
}

/* next call will return the next file name part. Returns 1 if successful, 0 at
   end of string, -1 for a too-long file name part. */
int get_next_part(char part[NAME_MAX + 1], const char** srcp) {
  const char* src = *srcp;
  char* dst = part;

  /* Skip leading slashes. If it's all slashes, we're done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;

  /* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
  while (*src != '/' && *src != '\0') {
    if (dst < part + NAME_MAX)
      *dst++ = *src;
    else
      return -1;
    src++;
  }
  *dst = '\0';

  /* Advance source pointer. */
  *srcp = src;
  return 1;
}

struct dir* dir_get_from_path(const char* directory) {
  struct thread* curr_thread = thread_current();

  if (strcmp(directory, "..") == 0)
    return dir_parent(curr_thread->dir);
  /* Absolute path */
  struct dir* curr_dir;
  if (directory[0] == '/' || curr_thread->dir == NULL)
    curr_dir = dir_open_root();
  /* Relative path */
  else
    curr_dir = dir_reopen(curr_thread->dir);

  /* Tokenize each directory */
  char dir_token[NAME_MAX + 1];
  while (get_next_part(dir_token, &directory) == 1) {
    /* Lookup directory from current directory */
    struct inode* next_inode;
    if (!dir_lookup(curr_dir, dir_token, &next_inode)) {
      dir_close(curr_dir);
      return NULL;
    }

    /* Open directory from inode received above */
    struct dir* next_dir = dir_open(next_inode);

    /* Close current directory and assign next directory as current */
    dir_close(curr_dir);
    if (!next_dir)
      return NULL;
    curr_dir = next_dir;
  }

  /* Return the last found inode if it is not removed */
  if (!inode_is_removed(dir_get_inode(curr_dir)))
    return curr_dir;

  dir_close(curr_dir);
  return NULL;
}