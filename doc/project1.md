# Design Document for Project 1: User-Prog

## Task 1: Argument Passing

### Data Structures and Functions

### Algorithms

### Synchronization

### Rationale
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

于是设计`extract_command_args`，`extract_command_name`调用`strtok_r`解析命令行参数，按照 8086 调用到约定将参数放入内存空间。

## Task 2: Process Control Syscalls

### System Call: int practice (int i)

和之前实现到传参类似，`f->esp`是栈指针，那么 args[0]是 SystemCall 的类型号，根据类型号，调用对应的 SystemCall 处理函数
同理`*(int*)(f->esp + 4)`就是 args[1]的值，
`f->eax`根据约定，存返回值的值，于是实现 practice 的 SystemCall，把 args[1]+1 传入 eax 为用户程序的返回值

```c
  int args[1] = *(int*)(f->esp + 4);
  f->eax = args[1] + 1;
```

然后 practice 就很好过了。

### System Call: void halt (void)

和前面的一样根据类型号，当判断是 halt 时调用`shutdown_power_off`关机

```c
    shutdown_power_off();
```

### System Call: void exit (int status)

根据类型号，当判断是 exit 时调用`thread_exit(status)`关闭进程;

```c
    thread_exit(status);
```

### System Call: pid t exec (const char \*cmd line)

exec 就要新建立一个进程，显然要调用`process_execute`，并且需要一个 list 来存储子进程，让父进程可以管理子进程。于是设计以下字段来保存信息

```c
  struct thread* parent; //父进程
  struct list children;//子进程
  struct semaphore exec_sema; //用于exec同步，只有当子进程load成功后，父进程才能从exec返回
  struct as_child_thread* pointer_as_child_thread;
```

当 SystemCall 时，调用`process_execute`创建新的进程，初始化线程时利用`thread_current`获得父线程 id，并完成对父进程 id 的初始化，初始化代码如下：

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

同时还需要更新父线程的 children 的 list，将子进程信息加入 list，代码如下：

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

父线程需要等待所有子线程，所以之前的线程的 children 派上用场，在 children 中遍历，找到对应 pid，然后利用之前设计的信号量等待子线程完成。
完成后从 list 中移除，并读取子线程返回值返回。

### 包装和用户空间判断

为了方便，对 SystemCall 处理函数进行了一些封装
同时要确保用户程序所提供的地址在用户程序的空间，要对其进行检测和处理，通过`get_user`函数判断空间是不是在用户空间。同时因该要检测如果是字符串输入字符串是不是合法（'\0'结尾），故设计如下函数进行判断：

```c
static bool is_valid_string(void* str) {
  int ch = -1;
  while ((ch = get_user((uint8_t*)str++)) != '\0' && ch != -1);
  if (ch == '\0')return true;
  else return false;
}
```

## Task 3: File Operation Syscalls

### System Call: bool create (const char \*file, unsigned initial size)

`filesys_create` 函数已经初步实现了，所以产生 create 的`systemcall`直接尝试调用`filesys_create`创建文件

### System Call: bool remove (const char \*file)

`filesys_remove` 函数已经初步实现了，所以产生 create 的`systemcall`尝试调用`filesys_remove`删除文件

### System Call: int open (const char \*file)

`filesys_open` 函数已经初步实现了，所以产生 create 的`systemcall`尝试调用`filesys_open`打开文件。由于要实现锁和打开后对读写操作，所以要将打开对文件加到线程维护对打开文件数组里。分配fd和添加到list代码如下：

```c
struct fd_entry* fd_entry = malloc(sizeof(struct fd_entry));
if (fd_entry == NULL) return -1;
fd_entry->fd = allocate_fd();
fd_entry->file = f;
list_push_back(&thread_current()->files, &fd_entry->elem);
```

### Get file from fd

从fd获得file需要遍历线程到file到list，来获得entry，代码如下

```c
  struct list* fd_table = &thread_current()->files;
  for (e = list_begin(fd_table); e != list_end(fd_table); e = list_next(e)) {
    struct fd_entry* tmp = list_entry(e, struct fd_entry, elem);
    if (tmp->fd == fd)
      return tmp;
  }
```

### System Call: int read (int fd, void \*buffer, unsigned size)

```c
int process_read(int fd, void* buffer, unsigned size) {
  if (fd == STDIN_FILENO) {
    //getbuf((char*)buffer, (size_t)size);
    return (int)size;
  } else if (get_fd_entry(fd) != NULL) {
    int si = file_read(get_fd_entry(fd)->file, buffer, size);
    return si;
  }
  return -1;
}
```

### System Call: int write (int fd, const void \*buffer, unsigned size)

```c
int process_write(int fd, const void* buffer, unsigned size) {
  if (fd == STDOUT_FILENO) {
    putbuf((char*)buffer, (size_t)size);
    return (int)size;
  } else if (get_fd_entry(fd) != NULL) {
    int si = file_write(get_fd_entry(fd)->file, buffer, size);
    return si;
  }
  return -1;
}
```

### System Call: int filesize (int fd)

在文件系统中 pintos 的`file_size`函数已经初步实现了，所以产生 seek 的`systemcall`时，利用传入的 fd，利用写好的`get_fd_entry`获取文件 entry，并且尝试调用`filesys_size`获取文件大小，并将返回值传入 eax。

### System Call: void seek (int fd, unsigned position)

在文件系统中 pintos 的`file_seek`函数已经初步实现了，和前几个一样调用函数，并传回返回值到 eax

### System Call: unsigned tell (int fd)

在文件系统中 pintos 的`file_tell`函数已经初步实现了，和前几个一样调用函数，并传回返回值到 eax

### System Call: void close (int fd)

在文件系统中 pintos 的`filesys_close` 函数已经初步实现了，和前几个一样调用函数，并传回返回值到 eax。同时也要将文件从线程到file的list中删除，以释放。

```c
     list_remove(&fd_entry->elem);
```

### 创建子线程限制

设定最大的线程数，防止系统因为线程过多无法调度，同时满足multi-oom的测试需求。

```c
  if (list_size(&all_list) >= 34) /* Maximum threads */
    return TID_ERROR;
```

### 读写同步问题

文件在执行一个文件操作的时候，例如在读取文件的时候不能写否则可能出现错误，简单起见应可视为一个原子操作，所以在每个对文件进行操作的函数加全局锁，可以基本上保证不会出现上述问题，解决一部分 rox 问题。
如

```c
    acquire_file_lock();
    int si = file_read(get_fd_entry(fd)->file, buffer, size);
    release_file_lock();
```

### 越界问题

当用户程序执行时候访问了不该访问的核心资源，应该kill，而不是让系统panic
修改exception.c，在页错误添加对访问限制资源对handle即添加is_kernel_vaddr(fault_addr)，进行异常处理。
