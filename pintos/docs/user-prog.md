# Project User-Prog

## Task 1: Argument Passing

根据`4.1.6 Program Startup Details`，命令行 `/bin/ls -l foo bar`解析到内存空间为
|地址|Name| Data | Type|
|---|---|----|----|
|...|...|...|...|
|0xbfffffcc|argv[0][...]|/bin/ls\0|char[8]|
|0xbfffffed|stack-align|0||
|...|...|...|...|
|0xbfffffd8|argv[0]|0xbfffffed||
|0xbfffffd4|argv|0xbfffffd8||
|0xbfffffd0|argc|4||
|0xbfffffcc|return address|0||

于是设计`extract_command_args`，`extract_command_name`调用`strtok_r`解析命令行参数，按照8086调用到约定将参数放入内存空间。核心代码如下：

```c
    int n = argc - 1;
    while (n >= 0) {//放入启动参数
      *esp = *esp - strlen(argv[n]) - 1;
      strlcpy(*esp, argv[n], strlen(argv[n]) + 1);
      address[n--] = *(char**)esp;
    }

    while ((int)(*esp - (argc + 3) * 4) % 16)//对齐，cs162特点，整个空间要对齐
      *esp = *esp - 1;

    argv[argc] = 0;
    *esp = *esp - 4;
    memcpy(*esp, &argv[argc], sizeof(char*));

    int t = argc - 1;
    while (t >= 0) {
      *esp = *esp - 4;
      memcpy(*esp, &address[t--], sizeof(char**)); //放入启动参数地址
    }

    void* argv0 = *esp;
    *esp = *esp - 4;
    memcpy(*esp, &argv0, sizeof(char**));//放入argv

    *esp = *esp - 4;
    memcpy(*esp, &argc, sizeof(int));//放入argc

    *esp = *esp - 4;//返回值地址0
```

## Task 2: Process Control Syscalls

### System Call: int practice (int i)

和之前实现到传参类似，`f->esp`是栈指针，那么args[0]是SystemCall的类型号，根据类型号，调用对应的SystemCall处理函数
同理`*(int*)(f->esp + 4)`就是args[1]的值，
`f->eax`根据约定，存返回值的值，于是实现practice的SystemCall，把args[1]+1传入eax

```c
  int args[1] = *(int*)(f->esp + 4);
  f->eax = args[1] + 1;
```

practice很好过

### System Call: void halt (void)

和前面的一样根据类型号，当判断是halt时调用`shutdown_power_off`关机

```c
    shutdown_power_off();
```

### System Call: void exit (int status)

根据类型号，当判断是exit时调用`thread_exit(status)`关闭进程;

```c
    thread_exit(status);
```

### System Call: pid t exec (const char *cmd line)

exec 就要新建立一个进程，显然要调用`process_execute`，并且需要一个list来存储子进程，让父进程可以管理子进程。于是设计以下字段来保存信息

```c
  struct thread* parent; //父进程
  struct list children;//子进程
  struct semaphore exec_sema; //用于exec同步，只有当子进程load成功后，父进程才能从exec返回
  struct as_child_thread* pointer_as_child_thread;
```

当SystemCall时，调用`process_execute`创建新的进程，初始化线程时利用`thread_current`获得父线程id，并完成对父进程id的初始化，初始化代码如下：

```c
#ifdef USERPROG
  list_init(&t->children);
  t->exit_status = UINT32_MAX;
  if (t == initial_thread)
    t->parent = NULL;
  else
    t->parent = thread_current();
#endif
```

同时还需要更新父线程的children的list，将子进程信息加入list，代码如下：

```c
#ifdef USERPROG
  //初始化孩子元素
  t->pointer_as_child_thread = malloc(sizeof(struct as_child_thread));
  t->pointer_as_child_thread->tid = tid;
  t->pointer_as_child_thread->exit_status = UINT32_MAX;
  t->pointer_as_child_thread->bewaited = false;
  sema_init(&t->pointer_as_child_thread->sema, 0);
  list_push_back(&thread_current()->children, &t->pointer_as_child_thread->child_thread_elem);
#endif
```

此外要判断需要执行的文件是否存在，不妨尝试能否打开该文件来检查是否存在，代码如下

```c
  acquire_file_lock();
  struct file* f = filesys_open(cmd_name); //检查是否存在
  if (f == NULL) {
    release_file_lock();
    palloc_free_page(fn_copy);
    free(cmd_name);
    return TID_ERROR;
  }
  file_close(f);
  release_file_lock();
```

### System Call: int wait (pid t pid)

父线程需要等待所有子线程，所以之前的线程的children派上用场，在children中遍历，找到对应pid，然后利用之前设计的信号量等待子线程完成。
完成后从list中移除，并读取子线程返回值返回。

### Warp

为了方便，对SystemCall处理函数进行了一些封装

## Task 3: File Operation Syscalls

### System Call: bool create (const char *file, unsigned initial size)
### System Call: bool remove (const char *file) 
### System Call: int open (const char *file)
### System Call: int filesize (int fd) 
### System Call: int read (int fd, void *buffer, unsigned size)
### System Call: int write (int fd, const void *buffer, unsigned size) 
### System Call: void seek (int fd, unsigned position)

### System Call: unsigned tell (int fd)

### System Call: void close (int fd)
## Additional Questions in cs162

1. Take a look at the Project 1 test suite in pintos/src/tests/userprog. Some of the test cases will intentionally provide invalid pointers as syscall arguments, in order to test whether your implementation safely handles the reading and writing of user process memory. Please identify a test case that uses an invalid stack pointer (%esp) when making a syscall. Provide the name of the test and explain how the test works. (Your explanation should be very specific: use line numbers and the actual names of variables when explaining the test case.)
2. Please identify a test case that uses a valid stack pointer when making a syscall, but the stack pointer is too close to a page boundary, so some of the syscall arguments are located in invalid memory. (Your implementation should kill the user process in this case.) Provide the name of the test and explain how the test works. (Your explanation should be very specific: use line numbers and the actual names of variables when explaining the test case.)
3. Identify one part of the project requirements which is not fully tested by the existing test suite. Explain what kind of test needs to be added to the test suite, in order to provide coverage for that part of the project. (There are multiple good answers for this question.)
