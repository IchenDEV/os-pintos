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
    while (n >= 0) {
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
      memcpy(*esp, &address[t--], sizeof(char**));
    }

    void* argv0 = *esp;
    *esp = *esp - 4;
    memcpy(*esp, &argv0, sizeof(char**));//argv

    *esp = *esp - 4;
    memcpy(*esp, &argc, sizeof(int));//argc

    *esp = *esp - 4;//返回值地址
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

### System Call: void halt (void)

根据类型号，当判断是halt时调用`shutdown_power_off`关机

```c
    shutdown_power_off();
```

### System Call: void exit (int status)

根据类型号，当判断是exit时调用`thread_exit(status)`关闭进程;

```c
    thread_exit(status);
```

### System Call: pid t exec (const char *cmd line)

### System Call: int wait (pid t pid)

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
