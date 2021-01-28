# FileSystem

## Cache

Cache本质就是把磁盘的内容按照块存储在内存中，故设计以下函数

```c
void cache_init(void);//初始化
void cache_flush(struct block* fs_device);//全部写回
void cache_read(...);//读缓存
void cache_write(...);//写缓存
```

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

### 集成

首先在文件系统初始化的时候调用cache的初始化函数，在文件系统释放的时候写回所有
对inode对读写原来是直接写在磁盘扇区，用cache的读写替换，使读写在cache完成。

## Grow

一开始的pintos，文件是只有直接块，且大小一开始就分配，所以为了支持大文件和便于扩展修改成直接块和二级间址

### 间接寻址，对磁盘inode结构体对修改

增加`indirect_block`和`double_indirect_block` 设计一级间址和二级间址,使文件大小空间可以扩充，并易于实现动态大小。
同理对`byte_to_sector`进行改造，使可以实现间地址访问

### allocate 空间

创建inode和写数据发现已经分配的不够时候要分配空间，
首先需要计算分配的空间范围，以确定需要的大小,按需求大小分配空间，即调用`free_map_allocate`从空闲块中选取一块空闲空间块，挂到inode对应的位置上。

### deallocate 空间

当删除文件时候应该对空间进行释放，以便于接下来利用。主要是文件删除的时候需要对文件空间进行释放，同理，遍历整个inode的直接块和间接块，对遍历到的空间块进行释放。

## subdir

实现子目录本质是实现一个特殊的文件，用来标示子文件。
按照以下结构存在磁盘块上

| 块 |
|-- |
|文件1/目录1|
|文件2/目录2|
|文件3/目录3|
|...|

### 对磁盘上块的结构调整

增加isdir字段用来标示是否为目录。
对应调整剩余区大小，使总大小不变，为一个块。

### 对向目录添加文件或者目录的实现

如果是添加一个目录，子目录内添加“..”作为父目录的指针，方便索引
其他和文件一样在父目录添加子文件或者文件夹的索引结构`dir_entry`指定子文件或者文件夹的位置。

### 当前目录和chdir

在thread结构体中添加current_dir对结构体指针，并初始为根目录。`chdir`即是对这个对修改。

### 对path的解析

对`filesys.c`进行改造，对`filesys_open`、`filesys_create`、`filesys_remove`等函数进行改造，对传入对path进行解析、分解，
如果是'\'开头，从根目录依次打开到所在文件目录，并分解出文件名，执行对应操作
如果是其他开头，从当前线程对当前目录开始依次打开，并分解文件名。

### readdir的实现

dir作为特殊文件，需要使用特殊的方式read，读取目录文件块的每一个dir_entry中包含的名字即可，核心如下

```c
bool dir_readdir(...) {
  struct dir_entry e;
  if (dir->pos == 0) {dir->pos = sizeof e;}/* 0 is parent dir */
  if (node_read_at(dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
    dir->pos += sizeof e;
    strlcpy(name, e.name, NAME_MAX + 1);
    return true;
  }
  return false;
}
```
