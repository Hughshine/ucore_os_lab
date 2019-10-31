Table of Contents
=================

* [lab3 Page Fault](#lab3-page-fault)
    * [练习0：填写已有实验](#练习0填写已有实验)
    * [练习1：给未被映射的地址映射上物理页](#练习1给未被映射的地址映射上物理页)
    * [练习2：补充基于FIFO的页面替换算法](#练习2补充基于fifo的页面替换算法)
    * [Challenge1：识别dirty bit的extended clock页替换算法](#challenge1识别dirty-bit的extended-clock页替换算法)
    * [Challenge2：不考虑实现开销和效率的LRU页算法](#challenge2不考虑实现开销和效率的lru页算法)
    * [附录](#附录)

# lab3 Page Fault

> 吐槽：
> 1. 以后一定要上完课再做，我哭了。（后面的似乎也没什么啦。。）
> 2. 理解关键的数据结构(very very very very very very important) + 函数用处，写下来会很轻松。
> 3. `diff` & `patch`

## 练习0：填写已有实验

不想再手动复制代码了，研究了一下如何使用`diff`&`patch`。有好多的坑，教程上还没有。

## pre_notes

记录一些核心的想法和重要的数据结构。此处查阅[ref](https://yuerer.com/%E6%93%8D%E4%BD%9C%E7%B3%BB%E7%BB%9F-uCore-Lab-3/)。

对于下面两个数据结构，mm_struct属于进程，vma_struct属于具体的虚拟内存块。

> 了解这样的一个含义，实验中见到的函数的功能就会很容易理解。

虚拟地址空间也需要进程去申请，struct vma_struct 用于记录虚拟内存块，很像物理内存中的Page。

```c++
struct vma_struct {
    struct mm_struct *vm_mm; // the set of vma using the same PDT (同一目录表，即同一进程)
    uintptr_t vm_start;      // start addr of vma 
    uintptr_t vm_end;        // end addr of vma, not include the vm_end itself
    uint32_t vm_flags;       // flags of vma
    list_entry_t list_link;  // linear list link which sorted by start addr of vma
};
```

mm_struct用于记录一个进程的虚拟地址空间，即一个进程拥有一个该struct实例。
```c++
struct mm_struct { // 描述一个进程的虚拟地址空间 每个进程的 pcb 中 会有一个指针指向本结构体
    list_entry_t mmap_list;        // 链接同一页目录表的虚拟内存空间 的 双向链表的 头节点（即这个进程的虚拟地址空间声明的[虚拟内存块的链表头]）
    struct vma_struct *mmap_cache; // 当前正在使用的虚拟内存空间，利用局部性优化
    pde_t *pgdir;                  // mm_struct 所维护的页表地址(拿来找 PTE)（一级页表地址）
    int map_count;                 // 虚拟内存块的数目
    void *sm_priv;                 // 记录访问情况链表头地址(用于置换算法)（给swap manager使用）
};
```

## 练习1：给未被映射的地址映射上物理页

本次lab的任务就是完成缺页异常的处理。缺页异常产生两个参数，即引发异常的目标虚拟地址，和错误码。在检查虚拟地址合法之后（是否在程序已申请的虚拟空间中？），将根据错误码决定后续操作。错误码有三种，分别表示是非法请求（权限异常，直接拒绝，goto failed），物理页被换出，没有分配物理页。练习一处理没有还没有分配物理页的情况。

1. 首先先找到虚拟地址对应的二级页表项。

> （若没有则分配，分配失败则failed）（当连分配二级页表的空间都没有时，此处会failed）。

2. 如果到这个物理地址的映射没有被初始化过（即对应的物理地址段全为0，没有记录硬盘存储信息），则创建映射。

> 如果存在了，就去调用练习二的页面替换算法

```c++
//vmm.c, do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
//函数含义：处理进程mm访问虚拟地址addr的产生的缺页异常，并给出了error_code.
// 函数中调用的，如 find_vma(mm, addr)都会很好理解了。
    ...
    // 找到二级页表项，只有当二级页表不存在，且内存不够给它分配时，ptep == NULL
    ptep = get_pte(mm->pgdir, addr, 1);              
    if(ptep == NULL)
        goto failed;
    if (*ptep == 0) { // 如果不存在到物理地址的映射，就创建这个映射
        struct Page *page = pgdir_alloc_page(mm->pgdir, addr, perm);//perm is short for permission
        if(page == NULL)
            goto failed;
    }
    else {
        //... 练习二
    }
```

## 再补充一点Notes

练习二的逻辑比较简单，但是先要明白ucore中swap的实现，以及设计的相关的数据结构。

由于swap动作本身很复杂，有很多算法，有内部状态，所以单独抽象出一个和`pmm_manager`, `vmm_manager` 同级别的 `swap_mamager`。

// TODO

## 练习2：补充基于FIFO的页面替换算法

## Challenge1：识别dirty bit的extended clock页替换算法



## Challenge2：不考虑实现开销和效率的LRU页算法


## 附录

lab3 与 lab2的差别：面向物理内存 or 面向虚拟内存。【lab2只有虚拟地址到物理地址的转换，分配物理内存，释放等】，不过没有建立物理内存与虚拟地址关系的过程。【更没有进一步的页面替换】。

现需要描述应用程序运行，所需的合法内存空间。page fault时获取应用程序的访问信息

VMA，virtual memory area

Integrated Drive Electronics (IDE) is a standard interface for connecting a motherboard to storage devices such as hard drives and CD-ROM/DVD drives. 

自映射机制：<https://www.cnblogs.com/richardustc/archive/2013/04/12/3015694.html>

页表项结构，标志位含义。其实产生了很大的差异。

![image-20191019195159649](/Users/lxy/Library/Application Support/typora-user-images/image-20191019195159649.png)

三个问题：

1. 当程序运行中访问内存产生page fault异常时，如何判定这个引起异常的虚拟地址内存访问是越界、写只读页的“非法地址”访问还是由于数据被临时换出到磁盘上或还没有分配内存的“合法地址”访问？
2. 何时进行请求调页/页换入换出处理？
3. 如何在现有ucore的基础上实现页替换算法？

页面的换入换出，实际上将缓存的页面看成了一级cache，所以要注意回写之类内容！【下面这个处理过程实际上是很简化的】

![image-20191019195948253](/Users/lxy/Library/Application Support/typora-user-images/image-20191019195948253.png)

要注意，需要重新执行产生缺页的指令！而不是从下一句继续执行。（是不是非致命性的异常都会重新执行那一句指令？）