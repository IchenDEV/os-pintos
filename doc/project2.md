# Design Document for Project 2: Thread

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

#### 总结

+ 没被捐赠过的，直接更新priority和original_priority两个变量
+ 正在被捐赠需要改优先级的且新优先级被当前优先级更低，更新original_priority
+ 正在被捐赠需要改优先级的且新优先级被当前优先级高，更新priority
+ 取消捐赠状态，恢复成旧优先级，更新priority

#### 释放互斥锁的操作

+ 该线程已经没有锁了：恢复捐赠前的优先级
+ 还有其它的锁：恢复成其它锁的最高优先级
+ 因为最后的锁而没有锁了，恢复捐赠前的优先级

## Task3 实现多级反馈队列调度算法

查阅资料发现目前pintos不支持浮点数运算，所以编写`fixed-point.h`，添加对浮点数运算对基本支持,实现最基本对加减乘除功能。

该算法的优先级是动态变化的，主要动态修改`Niceness`, `Priority`, `recent_cpu`, `load_avg`

Priority的计算公式为

    priority= PRI_MAX - (recent_cpu/ 4) - (nice*2)

每四个tick更新一次

recent_cpu的计算公式为

    recent_cpu= (2*load_avg)/(2*load_avg+1) *recent_cpu+nice

当`timer_ticks () % TIMER_FREQ == 0`时对所有线程更新，每个tick对当前线程的recent_cpu加1。

load_avg的计算公式为

    load_avg= (59/60)*load_avg+ (1/60)*ready_threads

当`timer_ticks () % TIMER_FREQ == 0`时对所有线程更新

### 同步

所有计算和排序都在timer_interrupt()中完成，其中中断被禁用。所以，同步不是问题。