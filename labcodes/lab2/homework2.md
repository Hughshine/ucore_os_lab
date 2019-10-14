# lab2 内存管理

[TOC]

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

```c
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

```c
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

### 优化

前面已经对`free`过程查找前后紧邻块做了优化。

如果对于每个空闲快，信息相同的保存在首与尾，那么在释放一个快时，就可以检查前一个page和后一个page是否是空闲的。如果前一个是空闲的，直接把本块信息清除即可；如果后一块是空闲的，把后一块的首page清除即可，此时`free`操作时常数时间。如果都不是，则需要用线性时间找到对应位置，插入块。

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

