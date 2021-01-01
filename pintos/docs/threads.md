# Project Threads

## Task 1: Efficient Alarm Clock

看一下时钟的实现

```C
  while (timer_elapsed(start) < ticks)
    thread_yield();
```

骨架代码中是一个循环不停检测是否可以执行否则 yield，显然效率不高，不妨利用中断完成。

```c
    intr_register_ext(0x20, timer_interrupt, "8254 Timer");
```

在代码中找到默认是配了一个8254的时钟中断`timer_interrupt`，所以不妨在检测到等待执行中阻塞进程让其sleep，发现`thread_block`函数描述符合需求，使用`thread_block`进行阻塞,利用时钟中断，在每次进入中断进行检测是否可以执行，如果可以执行，调用`thread_unblock`解除阻塞。

于是在线程`thread.h`添加`ticks_blocked`用于记录需要block的tick数量，在`timer_interrupt`中每次`ticks_blocked`减1，阻塞到0时解除阻塞，加入到ready队列。

## Task 2: Priority Scheduler

线程调度