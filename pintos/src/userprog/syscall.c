#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"

typedef int pid_t;
static int (*syscall_handlers[20])(struct intr_frame*); /* Array of syscall functions */
struct lock file_system_lock;

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int get_user(const uint8_t* uaddr) {
  if (!is_user_vaddr(uaddr))
    return -1;
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:" : "=&a"(result) : "m"(*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool put_user(uint8_t* udst, uint8_t byte) {
  if (!is_user_vaddr(udst))
    return false;
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:" : "=&a"(error_code), "=m"(*udst) : "q"(byte));
  return error_code != -1;
}

static bool is_valid_string(void* str) {
  int ch = -1;
  while ((ch = get_user((uint8_t*)str++)) != '\0' && ch != -1)
    ;
  if (ch == '\0')
    return true;
  else
    return false;
}

static bool is_valid_pointer(void* esp, uint8_t argc) {
  uint8_t i = 0;
  for (; i < argc; ++i)
    if (get_user(((uint8_t*)esp) + i) == -1)
      return false;
  return true;
}

static void kill_program(void) { thread_exit(-1); }

static void syscall_handler(struct intr_frame* f UNUSED) {

  /*
   * The following print statement, if uncommented, will print out the syscall
   * number whenever a process enters a system call. You might find it useful
   * when debugging. It will cause tests to fail, however, so you should not
   * include it in your final submission.
   */

  /* printf("System call number: %d\n", args[0]); */
  if (!is_valid_pointer(f->esp, 4)) {
    kill_program();
    return;
  }
  uint32_t* args = ((uint32_t*)f->esp);

  if (args[0] < 0 || args[0] >= 20) {
    kill_program();
    return;
  }
  int res = syscall_handlers[args[0]](f);

  if (res == -1) {
    kill_program();
    return;
  }
}

static int syscall_exit(struct intr_frame* f) {
  int status;
  if (!is_valid_pointer(f->esp + 4, 4))
    return -1;
  else
    status = *((int*)f->esp + 1);
  thread_exit(status);
  return 0;
}
static int syscall_practice(struct intr_frame* f) {
  int status;
  if (!is_valid_pointer(f->esp + 4, 4))
    return -1;
  status = *((int*)f->esp + 1);
  int cx = *(int*)(f->esp + 4);
  f->eax = cx + 1;
  return 0;
}

static int syscall_write(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 12))
    return -1;

  int fd = *(int*)(f->esp + 4);
  void* buffer = *(char**)(f->esp + 8);
  unsigned size = *(unsigned*)(f->esp + 12);
  if (!is_valid_pointer(buffer, 1) || !is_valid_pointer(buffer + size, 1))
    return -1;
  int written_size = process_write(fd, buffer, size);
  f->eax = written_size;
  return 0;
}

static int syscall_read(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 12))
    return -1;

  int fd = *(int*)(f->esp + 4);
  void* buffer = *(char**)(f->esp + 8);
  unsigned size = *(unsigned*)(f->esp + 12);

  if (!is_valid_pointer(buffer, 1) || !is_valid_pointer(buffer + size, 1))
    return -1;
  int _size = process_read(fd, buffer, size);
  f->eax = _size;
  return 0;
}

static int syscall_halt(struct intr_frame* f UNUSED) {
  shutdown_power_off();
  return 0;
}
static int syscall_seek(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 8))
    return -1;

  int fd = *(int*)(f->esp + 4);
  unsigned pos = *(unsigned*)(f->esp + 8);
  acquire_file_lock();
  process_seek(fd, pos);
  release_file_lock();
  return 0;
}

static int syscall_tell(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 4))
    return -1;

  int fd = *(int*)(f->esp + 4);
  acquire_file_lock();
  f->eax = process_tell(fd);
  release_file_lock();
  return 0;
}
static int syscall_wait(struct intr_frame* f) {
  pid_t pid;
  if (is_valid_pointer(f->esp + 4, 4))
    pid = *((int*)f->esp + 1);
  else
    return -1;
  f->eax = process_wait(pid);
  return 0;
}
static int syscall_open(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 4) || !is_valid_string(*(char**)(f->esp + 4)))
    return -1;

  char* str = *(char**)(f->esp + 4);
  f->eax = process_open(str);
  return 0;
}
static int syscall_close(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 4))
    return -1;

  int fd = *(int*)(f->esp + 4);
  process_close(fd);
  return 0;
}
static int syscall_create(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 4) || !is_valid_string(*(char**)(f->esp + 4)) ||
      !is_valid_pointer(f->esp + 8, 4)) {
    return -1;
  }
  char* str = *(char**)(f->esp + 4);
  unsigned size = *(int*)(f->esp + 8);
  acquire_file_lock();
  f->eax = filesys_create(str, size, false);
  release_file_lock();
  return 0;
}
static int syscall_remove(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 4) || !is_valid_string(*(char**)(f->esp + 4)))
    return -1;

  char* str = *(char**)(f->esp + 4);
  acquire_file_lock();
  f->eax = filesys_remove(str);
  release_file_lock();
  return 0;
}
static int syscall_exec(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 4) || !is_valid_string(*(char**)(f->esp + 4)))
    return -1;

  char* str = *(char**)(f->esp + 4);
  // make sure the command string can fit into a page
  if (strlen(str) >= PGSIZE) {
    printf("very large strings(>PGSIZE) are not supported\n");
    return -1;
  }
  // non empty string and it does not start with a space(args delimiter)
  if (strlen(str) == 0 || str[0] == ' ') {
    printf("the command string should be non-empty and doesn't start with a space %s\n", str);
    return -1;
  }
  f->eax = process_execute(str);
  return 0;
}

static int syscall_filesize(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 4))
    return -1;
  int fd = *(int*)(f->esp + 4);
  acquire_file_lock();
  f->eax = process_filesize(fd);
  release_file_lock();
  return 0;
}

static int syscall_mkdir(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 4) || !is_valid_string(*(char**)(f->esp + 4)))
    return -1;
  char* str = *(char**)(f->esp + 4);
  acquire_file_lock();
  f->eax = filesys_create(str, 0, false);
  release_file_lock();
  return 0;
}
static int syscall_chdir(struct intr_frame* f) {
  if (!is_valid_pointer(f->esp + 4, 4) || !is_valid_string(*(char**)(f->esp + 4)))
    return -1;
  char* str = *(char**)(f->esp + 4);
  acquire_file_lock();
  f->eax = filesys_chdir(str);
  release_file_lock();
  return 0;
}
void syscall_init(void) {
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
  syscall_handlers[SYS_WRITE] = &syscall_write;
  syscall_handlers[SYS_READ] = &syscall_read;
  syscall_handlers[SYS_HALT] = &syscall_halt;
  syscall_handlers[SYS_WAIT] = &syscall_wait;
  syscall_handlers[SYS_EXIT] = &syscall_exit;
  syscall_handlers[SYS_CREATE] = &syscall_create;
  syscall_handlers[SYS_REMOVE] = &syscall_remove;
  syscall_handlers[SYS_CLOSE] = &syscall_close;
  syscall_handlers[SYS_PRACTICE] = &syscall_practice;
  syscall_handlers[SYS_SEEK] = &syscall_seek;
  syscall_handlers[SYS_TELL] = &syscall_tell;
  syscall_handlers[SYS_FILESIZE] = &syscall_filesize;
  syscall_handlers[SYS_OPEN] = &syscall_open;
  syscall_handlers[SYS_EXEC] = &syscall_exec;

  syscall_handlers[SYS_MKDIR] = &syscall_mkdir;
  syscall_handlers[SYS_CHDIR] = &syscall_chdir;
}
