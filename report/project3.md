# Final Report for Project 3: FileSystem

## Cache

### cache_init

完成对cache的初始化，将cache的内容完成初始化和对锁初始化，使cache内容初始化为无效块（空块）。

### cache_flush

将cache写过的内容（dirty）全部写回磁盘，对应扇区。
遍历每一个cahce，如果发现是dirty的调用：
    block_write(fs_device, cache[index].disk_sector_index, cache[index].data);
写回磁盘。

### cache_replace

当读、写不在cache内的扇区时，应该将内存中的进行替换，按照要求选用了Clock算法来确定要替换的扇区。
Clock算法核心如下：

```c
 /* Perform clock algorithm to find slot to evict. */
  while (true) {
    i = clock_position;
    clock_position++;
    clock_position %= CACHE_NUM_ENTRIES;

    lock_acquire(&cache[i].cache_block_lock);

    if (!cache[i].valid) {lock_release(&cache_update_lock);return i;}

    if (cache[i].chances_remaining == 0)break;

    cache[i].chances_remaining--;
    lock_release(&cache[i].cache_block_lock);
  }
```


## Grow

一开始的pintos，文件是只有直接块，且大小一开始就分配，所以为了支持大文件和便于扩展修改成直接块和二级间址

## subdir

实现子目录本质是实现一个特殊的文件，用来标示子文件。

### 添加用于令牌化字符串的函数

static int get_next_part（字符部分[NAME_MAX+1]，常量字符**srcp）；
将文件名部分从SRCP提取到Part中，并更新SRCP以便下一次调用返回下一个文件名部分。如果成功，则返回1，字符串末尾返回0，过长文件名部分返回-1。
添加用于将路径拆分到其目录和文件名的功能

bool split_directory_and_filename（const char *path，char *directory，char *filename）；
给定一个完整的路径，将目录和文件名提取到提供的指针中。
