# Final Report for Project 2: Threads

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

因此在线程设置完优先级之后应该立刻重新调度，因此只需要在thread_set_priority()函数里添加thread_yield()函数即可。
创建一个新的高优先级线程抢占当前线程，因此在thread_create中，如果新线程的优先级高于当前线程优先级，调用thread_yield()函数即可。
修改sema_up在waiters中取出优先级最高的thread，并yield()即可即可。
条件变量也维护了一个waiters用于存储等待接受条件变量的线程，那么就修改cond_signal（）函数唤醒优先级最高的线程即可。

### 捐赠优先级

这个是pintos中需要实现的一种优先级调整策略

#### 单独捐赠的情况

当高优先级线程因为低优先级线程占用资源而阻塞时，应将低优先级线程的优先级提升到等待它所占有的资源的最高优先级线程的优先级，让其先执行。

#### 多重捐赠的情况

由于每次捐赠的时候都是因为优先级高的一个进程需要申请一个握在优先级比较低的线程,因此维护一个lock_priority,记录获得这个锁的线程此时的优先级,线程可能会接受几个不同的优先级,因此需要在锁中,而不是在线程的结构中维护这样一个信息,以在释放锁,undonate 的时候能够将线程优先级恢复到正确的值。

#### 嵌套捐赠的情况

通过检测被捐赠的线程是否已经获得了所需要的全部锁来判断是否出现嵌套捐赠的情况，如是则设置好参数来进行下一轮的优先级捐赠。

## Task3 实现多级反馈队列调度算法

所以修改线程结构，在线程结构中添加变量：

```c
    fixed_point_t recent_cpu，初始化为0
    int nice;
```

接下来就是在每次中断时对这些值进行更新，修改timer.c文件,在`timer_interrupt`中加入对mlfqs相关值对计算，修改如下：

```c
static void timer_interrupt(......) {
  ......
  if (thread_mlfqs) {
    thread_mlfqs_increase_recent_cpu_by_one();
    if (ticks % TIMER_FREQ == 0)
      thread_mlfqs_update_load_avg_and_recent_cpu();
    else if (ticks % 4 == 0)
      thread_mlfqs_update_priority(thread_current());
  }
 .....
}
```

于是实现对应的计算函数，具体见代码，报告中略。

### 同步

所有计算和排序都在timer_interrupt()中完成，其中中断被禁用。所以，同步不是问题。
