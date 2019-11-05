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
    pde_t *pgdir;                  // mm_struct 所维护的页表地址(拿来找 PTE)（一级页表 PDT 地址）
    int map_count;                 // 虚拟内存块的数目
    void *sm_priv;                 // 记录访问情况链表头地址(用于置换算法)（给swap manager使用）[swap manager维护的实例]
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
    // call alloc_page & page_insert functions to allocate a page size memory & setup an addr map pa<--->la with linear address la and the PDT pgdir
        struct Page *page = pgdir_alloc_page(mm->pgdir, addr, perm);//perm is short for permission
        if(page == NULL)
            goto failed;
    }
    else {
        //... 练习二
    }
```

回答问题：

1. 请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。

> 页目录项：就是一级页表嘛，用来找到二级页表项。。
> 页表项：存储映射关系，在需要页的换入换出时，用于辅助记录磁盘上的位置。

2. 如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

> 这一练习开始时提到了。

## 再补充一点Notes

练习二的逻辑比较简单，但是先要明白ucore中swap的实现，以及设计的相关的数据结构。

由于swap动作本身很复杂，有很多算法，有内部状态，所以单独抽象出一个和`pmm_manager`, `vmm_manager` 同级别的 `swap_mamager`。

在swap过程中，我们首先要知道目标页被换到硬盘的位置，磁盘上要换走谁。ucore中，用物理地址的前24位保存硬盘存储位置的首扇区（共八个扇区，0.5kb一个扇区）（并且设计简化的对应关系：虚拟页对应的PTE的索引值 = swap page的扇区起始位置*8（进而Page需要额外记录引用它的虚拟地址））。根据不同算法，PRA的数据结构维护方式不同，对于FIFO算法，我们将维护一个（时序）（双向）链表，换走时换走头部的，并将被换入的放入头部之前（也即链表尾部）。

为了支持FIFO，PRA，对物理块额外建立一个链表，在Page struct上添加属性`pra_page_link`，并额外的使用`pra_vaddr`记录物理页对应的虚拟地址（首地址），用于确定换出的物理磁盘扇面号.

```c++
struct Page {  
……   
list_entry_t pra_page_link;   
uintptr_t pra_vaddr;   
};
```

对于`swap_mamager`，记录一下它各个属性的功能。其中`map_swappable()`，用于将一个页面加入FIFO算法队列，`swap_out_victim()`用于清空一个page，它在page_fault中断时alloc_page()函数中调用。（这个alloc_page调用pmm_manager->alloc_page()，是优先级更高一层的函数）。

> 其他函数用于extended clock算法等，先不提及啦。

```c++
struct swap_manager  
{  
    const char *name;  
    /* Global initialization for the swap manager */  
    int (*init) (void);  
    /* Initialize the priv data inside mm_struct */  
    int (*init_mm) (struct mm_struct *mm);  
    /* Called when tick interrupt occured */  
    int (*tick_event) (struct mm_struct *mm);  
    /* Called when map a swappable page into the mm_struct */  
    int (*map_swappable) (struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in); 
    /* When a page is marked as shared, this routine is called to delete the addr entry from the swap manager */
    int (*set_unswappable) (struct mm_struct *mm, uintptr_t addr);  
    /* Try to swap out a page, return then victim */  
    int (*swap_out_victim) (struct mm_struct *mm, struct Page *ptr_page, int in_tick);  
    /* check the page relpacement algorithm */  
    int (*check\_swap)(void);   
};
```

## 练习2：补充基于FIFO的页面替换算法

上面的page_alloc_page已经调用了swap_out[将victim的页表项处理好了（见`swap.c/swap_out()`）]。这一步不用再管他直接从硬盘读入即可。

如注释。

```c++
...
if(swap_init_ok) {
            struct Page *page=NULL;
            swap_in(mm, addr, &page); // 根据pte上信息，将page从硬盘换入
            // swap_in 调用 alloc_page, 进而间接调用swap_out_victim
            page_insert(mm->pgdir, page, addr, perm); // 更新PTE对应项，建立线性地址与物理地址的映射
            page->pra_vaddr = addr; // 记录物理页对应的虚拟地址，以用于硬盘后续的换入换出
            swap_map_swappable(mm, addr, page, 0); // 将这一页加入FIFO的链表
        }
        else {
            cprintf("no swap_init_ok but ptep is %x, failed\n",*ptep);
            goto failed;
        }
...
```

两个算法的实现，也都相对简单。

```c++
static int
_fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    list_entry_t *entry=&(page->pra_page_link);
 
    assert(entry != NULL && head != NULL);
    list_add_before(head, entry);// 插入末尾
    return 0;
}

static int
_fifo_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    assert(head != NULL);
    assert(in_tick==0);
    list_entry_t *first = list_next(head); //删去头
    list_del(first);
    *ptr_page = le2page(first, pra_page_link); //将这个page返回，用于找到va，找到对应页表项，并修改victim的页表项。
    return 0;
}
```

问题：

如果要在ucore上实现”extended clock页替换算法”请给你的设计方案，现有的swap_manager框架是否足以支持在ucore中实现此算法？如果是，请给你的设计方案。如果不是，请给出你的新的扩展和基此扩展的设计方案。并需要回答如下问题：

* 需要被换出的页的特征是什么？

> 时钟算法：相当于在FIFO基础上，仅跳过访问过的页。
> extended clock: 回写代价高，所以在看页是否被回写的基础上，再考虑是否被访问。
> 标记位再pte上

>  寻找顺序：不脏又没访问过的页 => 访问但没修改的页 => 访问了修改了的页

* 在ucore中如何判断具有这样特征的页？

```c++
// A => Access, D => Dirty
!(*ptep & PTE_A) && !(*ptep & PTE_D)  没被访问过 也没被修改过
(*ptep & PTE_A) && !(*ptep & PTE_D) 被访问过 但没被修改过
!(*ptep & PTE_A) && (*ptep & PTE_D) 没被访问过 但被修改过
```

* 何时进行换入和换出操作？

> 在缺页时换入；在物理页达到某个下限阈值时换出。

## Challenge1：识别dirty bit的extended clock页替换算法

## Challenge2：不考虑实现开销和效率的LRU页算法


## 附录

一些没有整理的笔记，暂时注释掉了。challenge想在八个lab都做完后再做。

<!-- lab3 与 lab2的差别：面向物理内存 or 面向虚拟内存。【lab2只有虚拟地址到物理地址的转换，分配物理内存，释放等】，不过没有建立物理内存与虚拟地址关系的过程。【更没有进一步的页面替换】。

现需要描述应用程序运行，所需的合法内存空间。page fault时获取应用程序的访问信息

VMA，virtual memory area

Integrated Drive Electronics (IDE) is a standard interface for connecting a motherboard to storage devices such as hard drives and CD-ROM/DVD drives. 

自映射机制：<https://www.cnblogs.com/richardustc/archive/2013/04/12/3015694.html>

页表项结构，标志位含义。其实产生了很大的差异。

页面的换入换出，实际上将缓存的页面看成了一级cache，所以要注意回写之类内容！【下面这个处理过程实际上是很简化的】

![image-20191019195948253](/Users/lxy/Library/Application Support/typora-user-images/image-20191019195948253.png)

要注意，需要重新执行产生缺页的指令！而不是从下一句继续执行。（是不是非致命性的异常都会重新执行那一句指令？） -->