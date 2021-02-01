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

### Algorithms

于是在线程`thread.h`添加`ticks_blocked`用于记录需要block的tick数量，在`timer_interrupt`中每次`ticks_blocked`减1，阻塞到0时解除阻塞，加入到ready队列。

## Task 2: Priority Scheduler

### Data Structures and Functions

#### 优先级

先看除去donation以外的测试。第一个测试priority-fifo的源代码如下。这个测试创建了一个优先级PRI_DEFAULT+2的主线程，并用这个线程创建了16个优先级PRI_DEFAULT+1的子线程，然后把主线程的优先级设置为优先级PRI_DEFAULT，所以现在pintos内有16个优先级PRI_DEFAULT+1的线程和1个优先级PRI_DEFAULT的线程在跑，测试需要把16个线程跑完再结束那一个线程。
#### 优先捐赠功能
当发现高优先级的任务因为低优先级任务占用资源而阻塞时，就将低优先级任务的优先级提升到等待它所占有的资源的最高优先级任务的优先级。对于优先级捐赠的这几个测试来说，有两个关键的问题，一是优先级嵌套，另一是因为互斥锁而导致的线程阻塞。并且这一系列的问题的解决并不是独立的。

设计old_priority保存之前优先级，使可以恢复

### Synchronization

处理的共享资源包括信号量等待列表、准备列表。通过在这些列表结构的每个定义中添加锁字段来同步。在修改列表之前和完成修改后获取锁即可。

### Rationale

因此在线程设置完优先级之后应该立刻重新调度，因此只需要在thread_set_priority()函数里添加thread_yield()函数即可。
创建一个新的高优先级线程抢占当前线程，因此在thread_create中，如果新线程的优先级高于当前线程优先级，调用thread_yield()函数即可。修改sema_up在waiters中取出优先级最高的thread，并yield()即可即可，也维护了一个waiters用于存储等待接受条件变量的线程，那么就修改cond_signal（）函数唤醒优先级最高的线程即可。

## Task3 实现多级反馈队列调度算法

### Data Structures and Functions

#### 浮点计算

借鉴一些资料，如下添加浮点计算数，实现简单的浮点计算

```c
/* Basic definitions of fixed point. */
typedef int fixed_t;
#define FP_SHIFT_AMOUNT 16
#define FP_CONST(A) ((fixed_t)(A << FP_SHIFT_AMOUNT))
#define FP_ADD(A, B) (A + B)
#define FP_ADD_MIX(A, B) (A + (B << FP_SHIFT_AMOUNT))
#define FP_SUB(A, B) (A - B)
#define FP_SUB_MIX(A, B) (A - (B << FP_SHIFT_AMOUNT))
#define FP_MULT_MIX(A, B) (A * B)
#define FP_DIV_MIX(A, B) (A / B)
#define FP_MULT(A, B) ((fixed_t)(((int64_t)A) * B >> FP_SHIFT_AMOUNT))
#define FP_DIV(A, B) ((fixed_t)((((int64_t)A) << FP_SHIFT_AMOUNT) / B))
#define FP_INT_PART(A) (A >> FP_SHIFT_AMOUNT)
#define FP_ROUND(A)                                                                              
  (A >= 0 ? ((A + (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT)                                
          : ((A - (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT))
```
#### 负载平均值中添加变量

    fixed_point_t load_avg;

#### 线程结构的修改

在线程结构中添加变量：

    fixed_point_t recent_cpu，初始化为0
    int nice;

最近CPU更新

    void recent_cpu_update(void);

计算 recent_cpu = (2 * load_avg)/(2 * load_avg + 1) * recent_cpu + nice

正如规范所说，可能必须单独计算系数以避免溢出。

#### 负载平均更新功能

    void load_avg_update(void);

计算为load_avg = (59/60) * load_avg + (1/60) * ready_threads
#### 负载平均获取器函数

    fixed_point_t get_load_avg(void);

只需返回 100 * load_average。
### Algorithms

首先mlfqs和捐赠优先级并不相同，如果启用thread_mlfqs，需要返回原始优先级。
否则，返回捐赠的优先级

#### 实现fixed_point_t get_recent_cpu()

只需返回recent_cpu变量，该变量在调用时应已计算并准确。
### Synchronization

所有计算和排序都在timer_interrupt()中完成，其中中断被禁用。所以，同步不是问题。
### Rationale

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
