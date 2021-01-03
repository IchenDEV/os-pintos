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
因此在线程设置完优先级之后应该立刻重新调度，因此只需要在thread_set_priority()函数里添加thread_yield()函数即可。
创建一个新的高优先级线程抢占当前线程，因此在thread_create中，如果新线程的优先级高于当前线程优先级，调用thread_yield()函数即可。
修改sema_up在waiters中取出优先级最高的thread，并yield()即可即可，修改如下

条件变量也维护了一个waiters用于存储等待接受条件变量的线程，那么就修改cond_signal（）函数唤醒优先级最高的线程即可，

## Task3
实现多级反馈队列调度算法
编写下面的文件fixed-point.h

1. 该算法的优先级是动态变化的，主要动态修改Niceness, Priority, recent_cpu, load_avg四大变量

2. Priority的计算公式为：priority= PRI_MAX - (recent_cpu/ 4) - (nice*2)，每四个clock tick对所有线程更新一次

3. recent_cpu的计算公式为recent_cpu= (2*load_avg)/(2*load_avg+ 1) *recent_cpu+nice，当timer_ticks () % TIMER_FREQ == 0时对所有线程更新，每个tick对当前线程的recent_cpu加1。

4. load_avg的计算公式为load_avg= (59/60)*load_avg+ (1/60)*ready_threads，当timer_ticks () % TIMER_FREQ == 0时对所有线程更新

接下来就是在每次中断时对这些值进行更新，修改timer.c文件