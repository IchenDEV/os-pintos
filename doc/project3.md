# Design Document for Project 3: FileSystem

## Cache

Cache本质就是把磁盘的内容按照块存储在内存中

### Data Structures and Functions

#### cache 大小的设定

```c
#define CACHE_NUM_ENTRIES 64
#define CACHE_NUM_CHANCES 1
```

#### cache 操作的基本函数

```c
void cache_init(void);//初始化
void cache_flush(struct block* fs_device);//全部写回
void cache_read(...);//读缓存
void cache_write(...);//写缓存
```

#### cache_init

完成对cache的初始化，将cache的内容完成初始化和对锁初始化，使cache内容初始化为无效块（空块）。

#### cache_flush

将cache写过的内容（dirty）全部写回磁盘，对应扇区。
遍历每一个cahce，如果发现是dirty的调用：
    block_write(fs_device, cache[index].disk_sector_index, cache[index].data);
写回磁盘。

#### cache_replace

当读、写不在cache内的扇区时，应该将内存中的进行替换，按照要求选用了Clock算法来确定要替换的扇区。

### Algorithms

#### Clock 替换算法

时钟置换算法可以认为是一种最近未使用算法，即逐出的页面都是最近没有使用的那个。我们给每一个页面设置一个标记位remaing，remaing=1表示最近有使用u=0则表示该页面最近没有被使用，应该被逐出。

#### cache_write

在检查任何cache_block的值之前，先获取cache_block的锁，并检测是否在cache中；
保存命中的索引→i；
设置dirty=true；
将写入数据复制到cache；
释放cache_block_lock

#### cache_read

在检查任何cache_block的值之前，先获取cache_block的锁，并检测是否在cache中；
保存命中的索引→i；
将数据复制到dest；
释放cache_block_lock

### Synchronization

写Cache要保证互斥,在每个任务cache_block检查或修改其数据之前获取锁。此外，cache_evict（）将在修改cache_update_lock之前获取该锁。

### Rationale

首先在文件系统初始化的时候调用cache的初始化函数，在文件系统释放的时候写回所有，对inode对读写原来是直接写在磁盘扇区，用cache的读写替换，使读写在cache完成。

## Grow

一开始的pintos，文件是只有直接块，且大小一开始就分配，所以为了支持大文件和便于扩展修改成直接块和二级间址

### Data Structures and Functions

#### 间接寻址，对磁盘inode结构体对修改

增加`indirect_block`和`double_indirect_block` 设计一级间址和二级间址,使文件大小空间可以扩充，并易于实现动态大小。
同理对`byte_to_sector`进行改造，使可以实现间地址访问

```c
#define DIRECT_BLOCK_COUNT 124
#define INDIRECT_BLOCK_COUNT 128
```

```c

struct inode_disk {
  block_sector_t direct_blocks[DIRECT_BLOCK_COUNT];
  block_sector_t indirect_block;
  block_sector_t doubly_indirect_block;

  bool is_dir;    /* Indicator of directory file */
  off_t length;   /* File size in bytes. */
  unsigned magic; /* Magic number. */
};
```

### allocate 空间

创建inode和写数据发现已经分配的不够时候要分配空间，
首先需要计算分配的空间范围，以确定需要的大小,按需求大小分配空间，即调用`free_map_allocate`从空闲块中选取一块空闲空间块，挂到inode对应的位置上。

### deallocate 空间

当删除文件时候应该对空间进行释放，以便于接下来利用。主要是文件删除的时候需要对文件空间进行释放，同理，遍历整个inode的直接块和间接块，对遍历到的空间块进行释放。

### Algorithms

#### 实现inode_allocate ()

    static bool inode_allocate (struct inode_disk *inode_disk, off_t length)

inode_allocate将在两个期间调用，inode_create期间调用一次，在 inode_write_at期间调用一次块将根据需要多少空间直接分配，

要调用inode_allocate创建文件，首先通过调用 bytes_to_sectors获得。从direct_blocks开始，通过调用来free_map_allocate块。当分配资源后需要分配direct_blocks时，indirect_block。indirect_block将有128个块。如果在分配资源后需要更多的块，则分配双间接块。计算需要分配的大小，并进行分配。

对于调用 inode_allocate 以扩展文件，inode_allocate将检查每个扇区，并且仅在以前未分配时进行分配。
使 length = 大小

#### bytes_to_sectors

```c
static block_sector_t byte_to_sector(const struct inode* inode, off_t pos) {

  if (pos < disk_inode->length) {

    /* direct block */
    if (index < DIRECT_BLOCK_COUNT)
     获取直接地址块所在号
    /* indirect block */
    else if (index < DIRECT_BLOCK_COUNT + INDIRECT_BLOCK_COUNT) {
    获取一级间接地址块所在号
    }
    /* doubly indirect block */
    else {
      获取二级间接地址块所在号
  }
  return sector;
}
```

### Synchronization

同步需要使free_map_allocate相关函数线程安全。我们为创建了一个free_map，当调用 free_map_allocate （） 和 free_map_release （） 时，将获取/释放该锁。由于 free_map 作为静态变量而不是文件访问，因此它不会通过缓存，因此用户必须通过锁，在读写的时候完成同步。
## subdir

实现子目录本质是实现一个特殊的文件，用来标示子文件。
按照以下结构存在磁盘块上

| 块 |
|-- |
|文件1/目录1|
|文件2/目录2|
|文件3/目录3|
|...|

### Data Structures and Functions

#### 对磁盘上块的结构调整

增加isdir字段用来标示是否为目录。
对应调整剩余区大小，使总大小不变，为一个块。

#### 当前目录

在thread结构体中添加current_dir对结构体指针，并初始为根目录。

### Algorithms

#### 实现sys_chdir ()

`bool sys_chdir (const char *dir)`

设置 thread_current()->current_dir = dir

#### 实现sys_mkdir ()

bool sys_mkdir (const char *dir)

使用修改后的filesys_create()创建目录

#### 实现 readdir ()

dir作为特殊文件，需要使用特殊的方式read，读取目录文件块的每一个dir_entry中包含的名字即可

bool readdir (int fd, char *name)

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


#### 实现isdir ()

    bool isdir (int fd)

我们在inode的结构中加入对目录还是文件的标示位，读取该位即可。

#### 实现inumber ()

int inumber (int fd)

我们将inode所在块号作为inumber，返回对应块号即可。

#### 对向目录添加文件或者目录的实现

如果是添加一个目录，子目录内添加“..”作为父目录的指针，方便索引
其他和文件一样在父目录添加子文件或者文件夹的索引结构`dir_entry`指定子文件或者文件夹的位置。

#### 对path的解析

对`filesys.c`进行改造，对`filesys_open`、`filesys_create`、`filesys_remove`等函数进行改造，对传入对path进行解析、分解，如果是'\'开头，从根目录依次打开到所在文件目录，并分解出文件名，执行对应操作如果是其他开头，从当前线程对当前目录开始依次打开，并分解文件名。

### Synchronization

每个dir结构都关联一个单独的锁。每当使用dir执行线程安全操作时，都会使用此锁。由于每个dir与目录树中的不同级别相关联，这允许我们确保仅在修改相同dir的操作上相互排斥。

### Rationale

目录路径中的每个标记部分都与自己的inode和dir结构相关联。通过将目录树中的每个级别视为单独的dir，我们可以递归遍历“树”，简化访问不同文件/目录的逻辑。此外，通过保留第一个dir条目作为父目录扇区的持有人，它允许我们在解析目录树时轻松处理“...”大小写。
