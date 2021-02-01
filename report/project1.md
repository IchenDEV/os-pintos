# Final Report for Project 1: User Programs

## Task 1: Argument Passing

### Algorithms

根据设计报告所描述，核心代码设计如下：

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

### 解析字符串

* strtok_r()被使用而不是strtok()。这允许线程安全并发解析。

## Task 2: Process Control Syscalls

对struct exit status 进一步改造

struct exit_status {pid_t pid;};

```c
//作为孩子元素的线程信息
// 当原来线程被摧毁之后，仍然存在。只有当父进程读到他结束的状态之后，才释放。
struct as_child_thread {
  tid_t tid;
  int exit_status;
  struct list_elem child_thread_elem;
  bool bewaited;
  struct semaphore sema;
};
```

等待状态结构现在封装单父子关系，并在父级和子级之间共享。 并允许最后一个成员在退出之前释放空间。因为它会给出一个错误成功的退出代码。添加了bewaited，以便处理不允许父级等待同一子级多个次的行为。例如，一旦父级等待该子级（因为我们知道该子级必须已死亡），我们考虑释放父级子级列表中的等待状态结构。
### page_fault (struct intr_frame *f) in exception.c

我们更改了页面错误的处理方式，调用exit(-1)，并将*eax设置为-1。当用户试图在不进行系统调用的情况下取消引用无效地址时，内核不会painc。

## Task 3: File Operation Syscalls

#### 对thread_create()的修改

向函数添加调用init_file_list()
Implementation of init_fd_list() in thread.c
