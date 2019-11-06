# lab2 内存管理

[TOC]

## 练习零：补充lab1

由于lab1没有entry.S建栈（设置了栈的范围），做一些检查，在`printstackframe()`函数中可以不设置递归终点`ebp!=0`。但是lab2设置了，所以`qemu`会死掉。所以要调整好这里。

手动补充的lab1代码。

```c
void
print_stackframe(void) {
     uint32_t ebp = read_ebp();
     uint32_t eip = read_eip();
      /*按照16进制输出，补齐8位的宽度，补齐位为0，默认右对齐*/
     for(int i=0; ebp != 0&&i<STACKFRAME_DEPTH; i++) {//lab2 change here
        cprintf("ebp:0x%08x ", ebp);
        cprintf("eip:0x%08x ", eip);

        uint32_t *args = (uint32_t *)ebp + 2;
        for(int j=0;j<4;j++) {
            cprintf("args[%d]:0x%08x ", j, args[j]);
        }

        cprintf("\n");   
        print_debuginfo(eip - 1);

        eip = ((uint32_t *)ebp)[1];
        ebp = ((uint32_t *)ebp)[0];
     }
}
```

## 练习一：first-fit 物理内存管理

### 初始化

`default_init()`不需要改变。建立空的双向链表，并设置空块总量为0。

```c
static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}
```

`default_init_memmap`，为方便，按地址从小到大构建链表，即每次将新的块插入后面。

```c
/**
 * 初始化时使用。
 * 探测到一个基址为base，大小为n 的空间，将它加入list（开始时做一点检查）
 */
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    // 按地址序，依次往后排列。因为是双向链表，所以头指针前一个就是最后一个。
    // 只改了这一句。
    list_add_before(&free_list, &(base->page_link)); 
}
```

### `alloc`

就是找到第一个足够大的页，然后分配它。主要是`free`时，没有保证顺序，所以分配时也是乱序的。这一段只需要改：拆分时小块的插入位置，就插在拆分前处，而不是在list最后即可。

```cpp
// 可以发现，现在的分配方法中list是无序的，就是根据释放时序。
// 取的时候，直接去找第一个可行的。
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    // 要的页数比剩余free的页数都多，return null
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    // 找了一圈后退出 TODO: list有空的头结点吗？有吧。
    while ((le = list_next(le)) != &free_list) {
        // 找到这个节点所在的基于Page的变量
        // 这里的page_link就是成员变量的名字，之后会变成宏。。看起来像是一个变量一样，其实不是。
        // ((type *)((char *)(ptr) - offsetof(type, member)))
        // #define offsetof(type, member)
        // ((size_t)(&((type *)0)->member))
        // le2page, 找到这个le所在page结构体的头指针，其中这个le是page变量的page_link成员变量
        struct Page *p = le2page(le, page_link);
        // 找到了一个满足的，就把这个空间（的首页）拿出来
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    //如果找到了可行区域
    if (page != NULL) {
        // 这个可行区域的空间大于需求空间，拆分，将剩下的一段放到list中【free+list的后面一个】
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            // 加入后来的，p
            list_add_after(&(page->page_link), &(p->page_link));
            // list_add(&free_list, &(p->page_link));
        }
        // 删除原来的
        list_del(&(page->page_link));
        // 更新空余空间的状态
        nr_free -= n;
        //page被使用了，所以把它的属性clear掉
        ClearPageProperty(page);
    }
    // 返回page
    return page;
}
```

### `free`

未修改前，可以发现算法是，从头找到尾部，找到是否有被free的块紧邻的块。而first fit算法是有序的，只需找到它的前后即可，然后合并放入对应位置。

```c
//在完整的list中找有没有恰好紧贴在这个块前面 或 后面的，如果有，贴一起。
// 最多做两次合并，因为list中的块是已经合并好的了，新加一块最多缝合一个缝隙
    while (le != &free_list) {
        p = le2page(le, page_link);
        le = list_next(le);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
        else if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            base = p;
            list_del(&(p->page_link));
        }
    }
    nr_free += n;
    // 将新块加如list
    // list_add(&free_list, &(base->page_link));
    le = list_next(&free_list);
    while (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property <= p) {
            assert(base + base->property != p);
            break;
        }
        le = list_next(le);
    }
    list_add_before(le, &(base->page_link));
```

修改后。

```c++
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    // 先更改被释放的这几页的标记位
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    // 将这几块视为一个连续的内存空间
    base->property = n;
    SetPageProperty(base);

    list_entry_t *next_entry = list_next(&free_list);
    // 找到base的前一块空块的后一块
    while (next_entry != &free_list && le2page(next_entry, page_link) < base)
        next_entry = list_next(next_entry);
    // 找到前面那块
    list_entry_t *prev_entry = list_prev(next_entry);
    // 找到insert的位置
    list_entry_t *insert_entry = prev_entry;
    // 如果和前一块挨在一起，就和前一块合并
    if (prev_entry != &free_list) {
        p = le2page(prev_entry, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            base = p;
            insert_entry = list_prev(prev_entry);
            list_del(prev_entry);
        }
    }
	// 后一块
    if (next_entry != &free_list) {
        p = le2page(next_entry, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(next_entry);
        }
    }
    // 加一下
    nr_free += n;
    list_add(insert_entry, &(base->page_link));
}
```

### 可能的优化方式

前面已经对`free`过程查找前后紧邻块做了优化。

如果对于每个空闲快，信息相同的保存在首与尾，那么在释放一个快时，就可以检查前一个page和后一个page是否是空闲的。如果前一个是空闲的，修改前一块的ref，然后直接把本块信息清除即可；如果后一块是空闲的，把后一块的首page、末page清除，并相应调整块大小。此时`free`操作时常数时间。如果都不是，则需要用线性时间找到对应位置，插入块。

### 测试结果

```
make qemu
输出
...
check_alloc_page() succeeded!
...
但是make grade还是0分。
```

## 练习二：实现寻找虚拟地址对应的页表项

寻找页表项步骤：

1. 在一级页表（页目录）中找到它的对应项，如果存在，直接返回。

2. 如果不存在，不要求创建，返回NULL。

3. 如果不存在，要求创建，alloc空间失败，返回NULL

4. 成功拿到一个page，将它清空，并设置它的引用次数为1（在pages数组中）。

5. 并在一级页表中建立该项。最后返回。

   > 注：返回的是pte的kernel virtual addr，它的计算方法是：
   >
   > 找到该线性地址的页目录项 ==> 
   >
   > 页目录项的内容为二级页表的地址，将它转换为虚拟地址 ==>
   >
   > 根据线性地址在二级页表中的偏移，找到对应页表项地址。

```cpp
pte_t *
get_pte(pde_t *pgdir, uintptr_t la, bool create) {
    // 段机制后得到的地址是linear_addr, ucore中目前va = la
    pde_t *pdep = &pgdir[PDX(la)]; // 找到它的一级页表项（指针），PDX，线性地址的前十位，page dir index
    if(!(*pdep & PTE_P)) // 看一级页表项，其实就是二级页表的物理地址，如果存在（证明二级页表）存在，在二级页表中找到，并直接返回
    {   
        if(!create) // 不要求create，直接返回
            return NULL;
        // 否则alloc a page，建立二级页表，（成功的话）并设置这个page的ref为1，将内存也清空。
        struct Page* page = alloc_page(); 
        if(page == NULL)
            return NULL;
        set_page_ref(page, 1); 
        uintptr_t pa = page2pa(page);  // 页清空
        memset(KADDR(pa), 0, PGSIZE);
        // 在一级页表中，设置该二级页表入口
        *pdep = (pa & ~0xFFF) | PTE_P | PTE_W | PTE_U;
    }
    // PDE_ADDR 就是取了个 &，因为设置的时候取了 |。 得到的是二级页表真正的物理地址。
    // (pte_t *)KADDR(PDE_ADDR(*pdep)): 将物理地址转换为 二级页表的核虚拟地址
    // [PTX(la)] 加上la中相对二级页表的偏移
    // 取地址，返回
    return &((pte_t *)KADDR(PDE_ADDR(*pdep)))[PTX(la)];  // (*pdep)是物理地址
}
```

问题：

- 请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中每个组成部分的含义以及对ucore而言的潜在用处。

  答：由于页目录项、页表项的低12位默认为0，所以可以作为标志字段使用。

  > | 位    | 意义                                                         |
  > | ----- | ------------------------------------------------------------ |
  > | 0     | 表项有效标志（PTE_U）// 初始化时设置，”User can access“标志  |
  > | 1     | 可写标志（PTE_W）// 初始化时设置，”Writeable“标志            |
  > | 2     | 用户访问权限标志（PTE_P）// 初始化时设置，”Present“标志，常用 |
  > | 3     | 写入标志（PTE_PWT）                                          |
  > | 4     | 禁用缓存标志（PTE_PCD）                                      |
  > | 5     | 访问标志（PTE_A）                                            |
  > | 6     | 脏页标志（PTE_D）                                            |
  > | 7     | 页大小标志（PTE_PS）                                         |
  > | 8     | 零位标志（PTE_MBZ）                                          |
  > | 11    | 软件可用标志（PTE_AVAIL）                                    |
  > | 12-31 | 页表起始物理地址/页起始物理地址                              |

- 如果ucore执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

  答：可能出现页异常的情况大致有：没有创建一个虚拟地址到物理地址的映射；创建了映射，但物理页不可写。发生了相应异常后，（也就是一种中断后），会保护CPU现场，分析中断原因，交给缺页中断处理，处理结束后恢复现场。额外的，缺页中断在返回后，需要重新执行产生中断的指令。

  > 参考：[here](<https://www.cnblogs.com/sunsky303/p/9214223.html>)

## 练习三：释放某虚地址所在页并取消对应二级页表项映射

查找步骤：

1. 检查对应二级页表项是否有效，无效则什么都不做。
2. 否则，对应二级页表的`ref--`，若`ref==0`，`free`该页。
3. 回写快表，置脏位。

```cpp
static inline void
page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep) {
    if (*ptep & PTE_P) { // 如果二级页表项存在
        struct Page *page = pte2page(*ptep); // 找到这个二级页表项对应的page
        if (page_ref_dec(page) == 0) // 自减该page的ref，如果为0，则free该page
            free_page(page);
        *ptep = 0; //将该page table entry置0
         // 先检查此时cpu使用的一级页表是不是pgdir，如果是，则在快表中，invalidate对应的线性地址。[la 是]
         // 如果不是，则它根本不在快表中。
         // 底层调用invlpg，[INVLPG m 使包含 m(地址) 的页对应TLB项目失效]
         // la应该就是page对应的线性地址吧。。
        tlb_invalidate(pgdir, la); 
    }
}
```

问题：

* 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

  > 答：有关系，每个页表项/页目录项都对应一个物理页，也就对应pages中的一项。

* 如果希望虚拟地址与物理地址相等，则需要如何修改lab2，完成此事？

  > TODO:  [参考了已有内容，没有手动实现，以后再尝试。]
  >
  > 1. 修改链接脚本 
  >
  > 2. `entry.S`中注释掉unmap va 0~4M
  >
  > 3. 修改`memlayout.h`中相关宏定义。 
  >
  >    ```c
  >    #define KERNBASE	0x0
  >    ```

## `make grade`

已经得到了50分。

```
(base) mbp-lxy:lab2 lxy$ make grade
Check PMM:               (3.0s)
  -check pmm:                                OK
  -check page table:                         OK
  -check ticks:                              OK
Total Score: 50/50
```

## `总结`

虽然`lab2`practice很少，但是递归学习很费时间，（要先看mooc，有很多参考资料，很多函数、宏需要看明白，并且指针的确是一个很绕的东西！）。

而且lab1 还留了个小bug，为了处理它还用了一段时间QAQ。

challenge先不做了，感觉做不完了。。TwT。。反正也没计入`grade`。

## CHALLENGE

> TODO

## CHANGELOG

20191016，12:27，完成基础practice，算上看慕课的时间大概10h。