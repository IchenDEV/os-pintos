#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
#ifndef USERPROG
#define USERPROG
#endif
#include "threads/thread.h"

#define CMD_ARGS_DELIMITER " "
#define CMD_ARGS_MAX 30
#define CMD_LENGTH_MAX 100

typedef int pid_t;

pid_t process_execute(const char* file_name);
int process_wait(tid_t child_tid);
void process_exit(int status);
void process_activate(void);
void process_init(void);
int process_open(const char* file_name);
int process_write(int fd, const void* buffer, unsigned size);
int process_read(int fd, void* buffer, unsigned length);
void process_close(int fd);
int process_read(int fd, void* buffer, unsigned length);
int process_write(int fd, const void* buffer, unsigned length);
void process_seek(int fd, unsigned position);
int process_filesize(int fd);
int process_tell(int fd);
#endif /* userprog/process.h */
