# lab4 内核线程创建 + 基本调度

## 练习0：填写已有实验

patch 都打不上了，什么鬼QAQ

## notes

记录一些重要的数据结构，没有很掌握的知识。

### `pro_struct`

```cpp
struct proc_struct {
    enum proc_state state; // Process state 进程所处状态，由于调度
    int pid; // Process ID
    int runs; // the running times of Proces
    uintptr_t kstack; // Process kernel stack 每个（kernel or client）线程都有一个内核栈，对于用户进程，该栈是在特权级改变使用于保存被打断的硬件信息的栈（2 pages in ucore）
    volatile bool need_resched; // need to be rescheduled to release CPU?
    struct proc_struct *parent; // the parent process 父亲进程。只有idleproc没有。
    struct mm_struct *mm; // Process's memory management field 由于内核线程常驻内存，因而在这里没有作用。
    struct context context; // Switch here to run process 实际上利用context进行上下文切换的函数是switch_to
    struct trapframe *tf; // Trap frame for current interrupt 用于再次被调度时进入中断
    uintptr_t cr3; // the base addr of Page Directroy Table(PDT) 记录页表起始地址，本应和mm->pgdir 一样
    uint32_t flags; // Process flag
    char name[PROC_NAME_LEN + 1]; // Process name
    list_entry_t list_link; // Process link list
    list_entry_t hash_link; // Process hash list
};
```

### kstack作用

> 记录了分配给该进程/线程的内核栈的位置。主要作用有以下几点。首先，当内核准备从一个进程切换到另一个的时候，需要根据kstack 的值正确的设置好 tss （可以回顾一下在实验一中讲述的 tss 在中断处理过程中的作用），以便在进程切换以后再发生中断时能够使用正确的栈。
> 其次，内核栈位于内核地址空间，并且是不共享的（每个线程都拥有自己的内核栈），因此不受到 mm 的管理，当进程退出的时候，内核能够根据 kstack 的值快速定位栈的位置并进行回收。
>（ucore的实现对栈溢出不敏感）

### tss作用

一个任务有一个task state segment, 用于记录任务状态。TR寄存器指向当前任务的TSS，任务切换时会将原寄存器内容写出到相应TSS，并将新的TSS内容填到寄存器中，以完成任务切换。

> TODO: 但感觉ucore中tss信息并没有起到那么大的作用，tss是否完全可以取代context?

```cpp

struct taskstate {
    ...
    uintptr_t ts_esp0;      // stack pointers and segment selectors
    ...
}
```


## 练习1：分配并初始化一个进程控制块

根据注释，将数值型属性设置为默认值，将指针设为NULL，将struct属性用`memset`清空。
```cpp
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0; 
        proc->kstack = NULL;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN + 1);
    }
    return proc;
}
```

问题：

* 请说明 struct context context和struct trapframe *tf 成员变量的含义 与 在本次实验中的作用。

> context就是进程调度时的核心寄存器环境
> 进程被调度时，是通过中断，并且调度发生在进入中断例程之前。所以一个进程被再次调度后，要率先进入中断，因而需要保存中断的信息，即trapframe。

```cpp
struct context {
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};
```  

## notes

两个全局变量。注意，进程加入hash_list的时候，一定要先get_pid().

* static list_entry_t hash_list[HASH_LIST_SIZE]：所有进程控制块的哈希表，proc_struct中的成员变量hash_link将基于pid链接入这个哈希表中。
* list_entry_t proc_list：所有进程控制块的双向线性列表，proc_struct中的成员变量list_link将链接入这个链表中。

> TODO: 还不清楚，它俩有什么用。
> local_intr_save/restore 这两个函数也不知道是做什么的，好像是要进行中断enable, disable的操作

## 练习2：为新创建的内核线程分配资源

按照注释去做。需要做的事情：

1. alloc_proc
2. 分配内核栈
3. （对于用户进程，）复制内存管理信息（内核进程共享内核空间）
4. 复制原进程上下文
5. 添加至进程列表
6. 唤醒新进程

```cpp
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    //    1. call alloc_proc to allocate a proc_struct
    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }
    proc->parent = current;  // 设置父进程
    //    2. call setup_kstack to allocate a kernel stack for child process
    if (setup_kstack(proc) != 0)
    {
        goto bad_fork_cleanup_proc;
    }
    //    3. call copy_mm to dup OR share mm according clone_flag
    if (copy_mm(clone_flags, proc) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }
    //    4. call copy_thread to setup tf & context in proc_struct
    copy_thread(proc, stack, tf);
    // bool intr_flag;
    //    5. insert proc_struct into hash_list && proc_list
    proc->pid = get_pid(); // 这一句话要在前面！！！
    hash_proc(proc);
    nr_process++;
    list_add(&proc_list, &(proc->list_link));
    //    6. call wakeup_proc to make the new child process RUNNABLE
    wakeup_proc(proc);
    //    7. set ret vaule using child proc's pid
    ret = proc->pid;
    
fork_out:
    return ret;
bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```

问题：能否保证为每个进程一个唯一的pid？

> 当然可以，否则就会出问题。
> 在get_pid函数中，有两个静态变量 last_pid 与 next_safe，两者间表示范围是safe的，并会一直维护。pid确定过程中，还会检查所有进程以确保pid是唯一的。（不过不知道会不会出现ISP问题。。感觉没有做同步）

### 练习3：阅读代码，理解 proc_run 函数和它调用的函数如何完成进程切换的。

```cpp
// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load base addr of "proc"'s new PDT
void
proc_run(struct proc_struct *proc) {
    if (proc != current) { // 如果还是自己，无动作
        bool intr_flag; 
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag); // 保存IF位，并禁止中断（因为ucore支持嵌套中断，异常，所以可能有禁止中断时被调度的可能？）
        {
            current = proc; // 更新进程指针
            load_esp0(next->kstack + KSTACKSIZE); // 更新TSS的栈顶指针指针
            lcr3(next->cr3); // load cr3
            switch_to(&(prev->context), &(next->context)); // 调用switch_to
        }
        local_intr_restore(intr_flag); // 恢复中断，以及IF标志位
    }
}
```

## `make grade`

```zsh
# make grade
Check VMM:               (2.0s)
  -check pmm:                                OK
  -check page table:                         OK
  -check vmm:                                OK
  -check swap page fault:                    OK
  -check ticks:                              OK
  -check initproc:                           OK
Total Score: 90/90
```

## Challenge 

> TODO

## 附

### `idleproc` 

```cpp
proc->state = PROC_UNINIT;  //设置进程为“初始”态
proc->pid = -1;             //设置进程pid的未初始化值
proc->cr3 = boot_cr3;       //使用内核页目录表的基址
...
idleproc->pid = 0;   // idleproc是第0个线程
idleproc->state = PROC_RUNNABLE;
idleproc->kstack = (uintptr_t)bootstack;  // 只有idle的kstack不需要分配，ucore已经初始化好了。
idleproc->need_resched = 1;  // idleproc 运行 cpu_idle, 只要有新的线程出现，立刻转交
set_proc_name(idleproc, "idle");
```


### `initproc`

创建结束后，从forkret处开始执行。