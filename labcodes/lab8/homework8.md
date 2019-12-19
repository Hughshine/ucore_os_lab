# lab8 文件系统

## 文件系统-note


文件系统为支持多样化的输入输出，其组件的实现有很高的层次性，并呈现给用户一个友好的接口。

在用户眼中，文件只是一个句柄，可以进行read/write，这是封装在c库中的通用文件系统的访问接口。调用会最终到达内核态，将句柄map为`struct file`，各种设备、不同文件系统中各种类型文件都被抽象为file，此时是在文件系统抽象层，file中呈现了文件的各种状态，并指向其在内存中的`inode`（是对不同文件系统中inode的抽象）。此处的`inode`是一个文件在数据意义上的根节点，其上存储了该文件的文件系统类型与在其文件系统上实际的`inode`表示（借助union），它的函数指针已经按照对应文件系统要求进行了赋值。此处可以理解为，（内存中的）file接口与inode接口，将VFS与其对应文件系统各种操作进行了绑定（映射），可以认为SFS是被包含在VFS里啦。

后续的读写操作，就要根据相应文件系统去实现啦。ucore支持两种文件系统，即设备与SFS。设备即stdin与stdout，它在内核初始化时，构造两者的内存inode结构与设备结构，常存于内核中。对于SFS，需要支持它的打开、读写、关闭操作。打开操作即根据路径的目录项逐个进入相应目录，找到对应文件；读文件时，会将在VFS层次读取文件至公共缓存（iob），再导向进程的缓存。

> 还有一些深入的细节。

> 另：ioctl, 一般用于设备输入输出，发送请求码（如弹出光驱等) 

## 重要数据结构的层次

```c++
// VFS 层
struct file {
    enum {
        FD_NONE, FD_INIT, FD_OPENED, FD_CLOSED,
    } status;                         //访问文件的执行状态
    bool readable;                    //文件是否可读
    bool writable;                    //文件是否可写
    int fd;                           //文件在filemap中的索引值
    off_t pos;                        //访问文件的当前位置              这个属性比较重要
    struct inode *node;               //该文件对应的内存inode指针
    int open_count;                   //打开此文件的次数
};

struct inode {
    union {
        struct device __device_info;
        struct sfs_inode __sfs_inode_info;
    } in_info;
    enum {
        inode_type_device_info = 0x1234,
        inode_type_sfs_inode_info,
    } in_type;
    int ref_count;
    int open_count;
    struct fs *in_fs;
    const struct inode_ops *in_ops;
};

// VFS, memory
struct sfs_inode {
    struct sfs_disk_inode *din;                     /* on-disk inode */
    uint32_t ino;                                   /* inode number */
    bool dirty;                                     /* true if inode modified */
    int reclaim_count;                              /* kill inode if it hits zero */
    semaphore_t sem;                                /* semaphore for din */
    list_entry_t inode_link;                        /* entry for linked-list in sfs_fs */
    list_entry_t hash_link;                         /* entry for hash linked-list in sfs_fs */
};

/* filesystem for sfs */
struct sfs_fs {
    struct sfs_super super;                         /* on-disk superblock */
    struct device *dev;                             /* device mounted on */
    struct bitmap *freemap;                         /* blocks in use are mared 0 */
    bool super_dirty;                               /* true if super/freemap modified */
    void *sfs_buffer;                               /* buffer for non-block aligned io */
    semaphore_t fs_sem;                             /* semaphore for fs */
    semaphore_t io_sem;                             /* semaphore for io */
    semaphore_t mutex_sem;                          /* semaphore for link/unlink and rename */
    list_entry_t inode_list;                        /* inode linked-list */
    list_entry_t *hash_list;                        /* inode hash linked-list */
};

// VFS, disk
struct sfs_disk_inode {
    uint32_t size;                                  /* size of the file (in bytes) */
    uint16_t type;                                  /* one of SYS_TYPE_* above */
    uint16_t nlinks;                                /* # of hard links to this file */
    uint32_t blocks;                                /* # of blocks */
    uint32_t direct[SFS_NDIRECT];                   /* direct blocks */
    uint32_t indirect;                              /* indirect blocks */
//    uint32_t db_indirect;                           /* double indirect blocks */
//   unused
};
```


## 练习1: 完成读文件操作的实现（需要编码）

请在实验报告中给出设计实现”UNIX的PIPE机制“的概要设方案，鼓励给出详细设计方案。

这里的函数关联的调用很复杂，需要明确每一个函数中每个参数的来源与作用。尤其是感觉ucore doc上说的不是人话，一句话反复读好几遍都看不懂TwT。。

> 写注释，看懂参数，递归了解每个函数如何被调用、参数来源与作用

下面都以读取为例。

```
file_read(fd, buffer, alen, &alen); // 调用fd2file，获得file结构，为VFS级别抽象
-> ret = vop_read(file->node, iob); //  (struct iobuf __iob, *iob = iobuf_init(&__iob, base, len,file->pos);) 
(-> sfs_read)
-> sfs_io(node, iob, 0)
-> sfs_io_nolock(sfs, sin, iob->io_base, iob->io_offset, &alen, write);
-> sfs_bmap_load_nolock, sfs_buf_op + sfs_block_op
```

在VFS抽象层，用户传入已打开文件的句柄，转换为struct file后，调用read或write，传入读取的buffer以及预期的读取长度。file->内存结构的SFS inode，重新封装了用户传入的buffer（iobuf，增加了关于读取的关键属性），准备好后开始真正的读写工作。
在sfs_io_nolock中，我们知道文件在硬盘上的索引块（sfs_disk_inode）中数据，知道要读取的起始点（offset）与读取长度（size）与用户的buffer（iobuf）。索引是呈现树状的，在读取文件逻辑上第i块时，我们需要先把它转换为磁盘上的inode序号（ucore中也就是硬盘的块序号）（借助sfs_bmap_load_nolock），并按需求将它读入用户buffer（借助sfs_buf_op + sfs_block_op）。

读取的内容可能有三个部分，首尾块可能是非块对齐的，中间连续块是对齐的。因而非对齐的首尾块，使用sfs_buf_op读取指定区域至buffer；对于完整的块，直接调用sfs_block_op将之读取至buffer。

根据以上思路，即可完成第一部分练习的代码。

```cpp
static int
sfs_io_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, void *buf, off_t offset, size_t *alenp, bool write) {
    // 本函数拿到的已经是正确的文件系统 以及file在文件系统中inode了（内存格式）
    // 希望把数据存入buf中，长度为offset
    // alenp是要返回的实际读取/写入的长度
    struct sfs_disk_inode *din = sin->din; // 将内存格式的sfs inode中 disk inode提取出来
    assert(din->type != SFS_TYPE_DIR); // 本文件不能是目录
    // 把文件当做tape，offset是目前读取的位置（起始点），加上要读取的长度得到endpos
    off_t endpos = offset + *alenp, blkoff;  // alenp 是预期长度， resident length，现根据它找到读取结束地址，最后传给它实际读取长度
    *alenp = 0;
	// calculate the Rd/Wr end position
    // 做一些检查，使起始地址与末尾地址在合法范围
    if (offset < 0 || offset >= SFS_MAX_FILE_SIZE || offset > endpos) {
        return -E_INVAL;
    }
    if (offset == endpos) { 
        return 0;
    }
    if (endpos > SFS_MAX_FILE_SIZE) {
        endpos = SFS_MAX_FILE_SIZE;
    }
    if (!write) {
        if (offset >= din->size) {  //如果起始点比文件size都大，直接返回
            return 0;
        }
        if (endpos > din->size) {  //结束点不能超过文件大小
            endpos = din->size;
        }
    }
    // 小范围的函数指针抽象、复用
    // sfs_buf_op(), 将第ino块（磁盘块号就是inode的序列号）从blkoff读取buf (或被写入)
    int (*sfs_buf_op)(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);
    // 以块为单位读取或写入至buff
    int (*sfs_block_op)(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
    if (write) {
        sfs_buf_op = sfs_wbuf, sfs_block_op = sfs_wblock;
    }
    else {
        sfs_buf_op = sfs_rbuf, sfs_block_op = sfs_rblock;
    }
    int ret = 0;
    size_t size, alen = 0;
    uint32_t ino;
    uint32_t blkno = offset / SFS_BLKSIZE;          // 起始块 块号
    uint32_t nblks = endpos / SFS_BLKSIZE - blkno;  // 起始块与结束块距离块数 The size of Rd/Wr blocks

  //LAB8:EXERCISE1 YOUR CODE HINT: call sfs_bmap_load_nolock, sfs_rbuf, sfs_rblock,etc. read different kind of blocks in file
	/*
	 * (1) If offset isn't aligned with the first block, Rd/Wr some content from offset to the end of the first block
	 *       NOTICE: useful function: sfs_bmap_load_nolock, sfs_buf_op
	 *               Rd/Wr size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset)
	 * (2) Rd/Wr aligned blocks 
	 *       NOTICE: useful function: sfs_bmap_load_nolock, sfs_block_op
     * (3) If end position isn't aligned with the last block, Rd/Wr some content from begin to the (endpos % SFS_BLKSIZE) of the last block
	 *       NOTICE: useful function: sfs_bmap_load_nolock, sfs_buf_op	
	*/
    if ((blkoff = offset % SFS_BLKSIZE) != 0)  { // 第一块是否是align的？
        // 要找到第一块中要读的大小。如果开始 开始块与结束块，块号相同，则只读 endpos - offset长度
        // 否则，读第一个块到结尾
        size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset);
       if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) // 载入这个文件逻辑上第blkno个数据块，ino为所对应的 索引
        {
            goto out;
        }
        if ((ret = sfs_buf_op(sfs, buf, size, ino, blkoff)) != 0) // 将第ino块（磁盘块号就是inode的序列号）从blkoff读取buf (或被写入)
        {
            goto out;
        }
        alen += size;
        if (nblks == 0)
        {
            goto out;
        }
        buf += size, blkno++;
        nblks--;
    }
    // 对齐的中间块，循环读取
    size = SFS_BLKSIZE;
    while (nblks != 0) { 
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        if ((ret = sfs_block_op(sfs, buf, ino, 1)) != 0) { // 读取或写入完整的一块
            goto out;
        }
        alen += size, buf += size, blkno++, nblks--;
    }
    // 末尾最后一块没对齐的情况
    // 和读取第一个块类似，先找到对应的索引号，再向buffer读取应读的大小
    if ((size = endpos % SFS_BLKSIZE) != 0) {  //更新size
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        if ((ret = sfs_buf_op(sfs, buf, size, ino, 0)) != 0) {   
            goto out;
        }
        alen += size;
    }
out:
    *alenp = alen;
    if (offset + alen > sin->din->size) {
        sin->din->size = offset + alen;
        sin->dirty = 1;
    }
    return ret;
}
```

**问题：给出UNIX的PIPE机制的概要设计方案**

> 管道的作用是，将一个进程的标准输出，接续至另一个文件的标准输入。在使用管道时，操作系统可声明一个临时文件，替换第一个进程的标准输出，并作为第二个进程的标准输入。PCB中记录这个文件是使用标准输入/标准输出还是从指定文件输入/指定文件输出。

## 练习2：完成基于文件系统的执行程序机制的实现（需要编码）

改写proc.c中的load_icode函数和其他相关函数，实现基于文件系统的执行程序机制。执行：make qemu。如果能看看到sh用户程序的执行界面，则基本成功了。如果在sh用户界面上可以执行”ls”,”hello”等其他放置在sfs文件系统中的其他执行程序，则可以认为本实验基本成功。

BSS段放置未初始化变量，表现为占位符，在载入系统时初始化。Data段放置被初始化的变量，不得不需要在可执行文件中记录。

```cpp
struct proghdr {
    uint32_t p_type;   // loadable code or data, dynamic linking info,etc.
    uint32_t p_offset; // file offset of segment
    uint32_t p_va;     // virtual address to map segment
    uint32_t p_pa;     // physical address, not used
    uint32_t p_filesz; // size of segment in file
    uint32_t p_memsz;  // size of segment in memory (bigger if contains bss）
    uint32_t p_flags;  // read/write/execute bits
    uint32_t p_align;  // required alignment, invariably hardware page size
};
```
**请在实验报告中给出设计实现基于”UNIX的硬链接和软链接机制“的概要设方案，鼓励给出详细设计方案。**


