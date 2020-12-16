#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "threads/fixed-point.h"

/* States in a thread's life cycle. */
enum thread_status {
  THREAD_RUNNING, /* Running thread. */
  THREAD_READY,   /* Not running but ready to run. */
  THREAD_BLOCKED, /* Waiting for an event to trigger. */
  THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */

//作为孩子元素的线程信息
// 当原来线程被摧毁之后，仍然存在。只有当父进程读到他结束的状态之后，才释放。
struct as_child_thread {
  tid_t tid;
  int exit_status;
  struct list_elem child_thread_elem;
  bool bewaited;
  struct semaphore sema;
};
//被某个线程打开的文件
struct opened_file {
  int fd;
  struct file* file;
  struct list_elem file_elem;
};

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
                  the run queue (thread.c), or it can be an element in a
                  semaphore wait list (synch.c).  It can be used these two ways
                  only because they are mutually exclusive: only a thread in the
                  ready state is on the run queue, whereas only a thread in the
                  blocked state is on a semaphore wait list. */
struct thread {
  /* Owned by thread.c. */
  tid_t tid;                 /* Thread identifier. */
  enum thread_status status; /* Thread state. */
  char name[16];             /* Name (for debugging purposes). */
  uint8_t* stack;            /* Saved stack pointer. */
  int priority;              /* Priority. */
  struct list_elem allelem;  /* List element for all threads list. */

  /* Shared between thread.c and synch.c. */
  struct list_elem elem; /* List element. */

#ifdef USERPROG
  /* Owned by userprog/process.c. */
  char* prog_name;

  uint32_t* pagedir; /* Page directory. */

  bool exec_success; //用于exec,判断子进程是否成功load its executable

  struct file* self_file; //自己这个可执行文件
  tid_t parent_tid;
  int next_fd;
  struct file* executable;
#endif
  struct thread* parent; //父进程
  struct list children;
  struct list files; //打开的文件
  struct semaphore exec_sema; //用于exec同步，只有当子进程load成功后，父进程才能从exec返回
  struct as_child_thread* pointer_as_child_thread;

  int exit_status; //退出状态
  /* Owned by thread.c. */

  struct list locks; /* Locks this thread holds */

  struct lock* waiting_lock; /* The lock this thread is waiting for */
  int original_priority;
  int nice;
  int recent_cpu;
  int64_t ticks_blocked;
  unsigned magic; /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void* aux);
tid_t thread_create(const char* name, int priority, thread_func*, void*);

void thread_block(void);
void thread_unblock(struct thread*);
void blocked_thread_check(struct thread* t, void* aux UNUSED);
struct thread* thread_current(void);
tid_t thread_tid(void);
const char* thread_name(void);

void thread_exit(int status) NO_RETURN;
void thread_yield(void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread* t, void* aux);
void thread_foreach(thread_action_func*, void*);

bool thread_compare_priority(const struct list_elem* a, const struct list_elem* b,
                             void* aux UNUSED);

/* Function for lock max priority comparison. */
bool lock_cmp_priority(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED);
void thread_hold_the_lock(struct lock* lock);
void thread_remove_lock(struct lock* lock);
void blocked_thread_check(struct thread* t, void* aux UNUSED);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

#ifdef USERPROG
/* Owned by userprog/process.c. */
struct thread* thread_get(tid_t tid);
bool thread_is_parent_of(tid_t tid);
#endif

#endif /* threads/thread.h */
