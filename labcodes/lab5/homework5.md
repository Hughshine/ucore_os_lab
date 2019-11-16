# lab5

> 平台问题，mac上 make grade 就不可以。具体原因还需要看一看。哭哭TwT。【后面的测试都不可以】。但在ubuntu上没有问题。
> 整的我都不知道是不是复制代码时复制错了。

## 练习0：补充代码

根据注释做需要的修改。

1. `alloc_proc`: 下面这三个指针，cptr应指向进程的首个孩子节点，孩子节点的兄弟都是该节点的孩子。兄弟关系是一个双向链表。【可能】由于父进程是一个经常被使用的项，所以为每个子进程都加上父进程指针。

```cpp
static struct proc_struct * alloc_proc(void) {
    //...
    //LAB5 YOUR CODE : (update LAB4 steps)
    /*
     * below fields(add in LAB5) in proc_struct need to be initialized	
     *       uint32_t wait_state;                        // waiting state
     *       struct proc_struct *cptr, *yptr, *optr;     // relations between processes
	 */
    proc->wait_state = 0;
    proc->cptr = proc->yptr = proc-> optr = NULL;
    // |   cptr: proc is child          |
    // |   yptr: proc is younger sibling |
    // |   optr: proc is older sibling   |
    //...
}
```

2. `do_fork`: 首先保证子进程在uninit状态，不会被调度。（虽然按`alloc_proc()`，感觉不会出现其他情况）。并`hash_proc(proc), set_links(proc)`将进程放入hash_list 与 proc_list。观察set_links可知。父进程的cptr永远指向最新的子进程，子进程的兄弟是按照时间排序的。

> hash list的作用是，根据pid去找对应进程。
> proc list用于调度

```cpp
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    ...
	//LAB5 YOUR CODE : (update LAB4 steps)
   /* Some Functions
    *    set_links:  set the relation links of process.  ALSO SEE: remove_links:  lean the relation links of process 
    *    -------------------
	*    update step 1: set child proc's parent to current process, make sure current process's wait_state is 0
	*    update step 5: insert proc_struct into hash_list && proc_list, set the relation links of process
    */
   if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }
    proc->parent = current; // 设置父进程
    assert(current->wait_state == 0);  
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
    //    5. insert proc_struct into hash_list && proc_list
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        // get_pid()是不会被打断的，所以一定可以保证不会出问题。
        proc->pid = get_pid(); // 这一句话要在前面！！！ 
        hash_proc(proc);
        // nr_process++; // set_links中已经做了++了。
        set_links(proc);  // TODO, 额外修改的部分
    }
    local_intr_restore(intr_flag);

    //    6. call wakeup_proc to make the new child process RUNNABLE
    wakeup_proc(proc);
    //    7. set ret vaule using child proc's pid
    ret = proc->pid;
//...
}
```

3. 时间片轮换，在每个时钟中断时，更改当前进程的状态，设置为可调度。

```cpp
static void trap_dispatch(struct trapframe *tf){
    //...
    case IRQ_OFFSET + IRQ_TIMER:
    ///...
        /* LAB5 YOUR CODE */
        /* you should upate you lab1 code (just add ONE or TWO lines of code):
         *    Every TICK_NUM cycle, you should set current process's current->need_resched = 1
         */
        if (++ticks % TICK_NUM == 0)
        {
            assert(current != NULL);
            current->need_resched = 1;
        }
    //...
}
```

4. idt初始化时，设置system call的idt discriptor的调用者为user。

> 是第一个作业的challenge做的事

```cpp
void
idt_init(void) {
     /* LAB1 YOUR CODE : STEP 2 */
     /* (1) Where are the entry addrs of each Interrupt Service Routine (ISR)?
      *     All ISR's entry addrs are stored in __vectors. where is uintptr_t __vectors[] ?
      *     __vectors[] is in kern/trap/vector.S which is produced by tools/vector.c
      *     (try "make" command in lab1, then you will find vector.S in kern/trap DIR)
      *     You can use  "extern uintptr_t __vectors[];" to define this extern variable which will be used later.
      * (2) Now you should setup the entries of ISR in Interrupt Description Table (IDT).
      *     Can you see idt[256] in this file? Yes, it's IDT! you can use SETGATE macro to setup each item of IDT
      * (3) After setup the contents of IDT, you will let CPU know where is the IDT by using 'lidt' instruction.
      *     You don't know the meaning of this instruction? just google it! and check the libs/x86.h to know more.
      *     Notice: the argument of lidt is idt_pd. try to find it!
      */
     cprintf("idt_init ok!");
     extern uintptr_t __vectors[];
     for (int i = 0; i < 256; ++i)
     {
         SETGATE(idt[i], 0, GD_KTEXT, __vectors[i], DPL_KERNEL);
     }
     SETGATE(idt[T_SYSCALL], 1, GD_KTEXT, __vectors[T_SYSCALL], DPL_USER);
     lidt(&idt_pd);
     /* LAB5 YOUR CODE */ 
     //you should update your lab1 code (just add ONE or TWO lines of code), let user app to use syscall to get the service of ucore
     //so you should setup the syscall interrupt gate in here
}
```

## 练习一：加载应用程序并执行（需要编码）

描述：do_execv函数调用load_icode（位于kern/process/proc.c中）来加载并解析一个处于内存中的ELF执行文件格式的应用程序，建立相应的用户内存空间来放置应用程序的代码段、数据段等，且要设置好proc_struct结构中的成员变量trapframe中的内容，确保在执行此进程后，能够从应用程序设定的起始执行地址开始执行。需设置正确的trapframe内容。


### 核心知识

#### 用户内存空间 与 核内存空间

首先要理解用户内存空间 与 核内存空间的关联。在32位linux系统中(ucore也如此)，给了内核1G的物理内存，用户态进程一起分享其他3G。由于内核的内存是所有进程共享的，它在初始化时，就建立了内核空间的映射，物理存在占低位1G，虚拟地址占最高位地址（最高的1G，加上0xC0000000）（不会发生pagefault等导致映射改变的事情）（尽管也存在一部分动态映射）。这一部分的页表项，对于所有进程（内核线程or用户进程）是相同的。一个用户进程在做syscall时，只做栈的转换，cr3不变。所以我们在谈内核虚拟空间时，就是说最高的1G虚拟地址，对于每个进程均完全一样。在谈用户虚拟地址时，说的是每个用户进程独立的低3G虚拟内存。独立的原因是页表的动态分配机制。

#### 预加工用户代码

一个用户进程，在编译时进一步包装，成了这个样子：

```
initcode(调整ebp, esp)，包含_start段 => 调用umain => 调用main 
```

#### 用户进程实际创建过程

1. `kernel_thread()` 做了什么

> kernel_thread将tf的eip设置为kernel_thread_entry，fn作为参数，新的内核线程执行 kernel_thread_entry => fn (比如说init_main, user_main).
> > 此处下面将忽略，就当kernel_thread直接执行了传入的函数。

2. `init_main()` 与 `user_main()` 做了什么
一个新的用户程序的创建、执行过程：

init_main => kernel_thread(user_main) => schedule() => forkrets(不起作用) =>  kernel_execve(显式调用syscall，想要加载执行指定elf格式代码) ==> system_exec => do_execv => load_icode(设置了帧，再次iret是可以回到elf->e_entry即__start) => trapret => __start 开始执行用户代码

* 其他要注意的内容：

> 应做区分的几个点：设置eip，是在do_fork时做，让新的子进程根据tf正确执行自己的代码。设置tf->eip（等），是在syscall中做，希望在此调用iret时可以回到用户进程的执行环境。
> 段寄存器cs, ds, esp等都被设置为用户的对应段。这些寄存器不能被改变（作为目的寄存器）。


### `load_icode`做的事情

	1. mm_create，为进程的数据管理结构申请（用户的）内存空间，并初始化。
	2. setup_pgdir 用来申请一个页目录表所需的一个页大小的内存空间。（并使进程的新目录表可以正确映射内核虚空间）
	3. 解析elf执行程序，通过mm_map，建立各个段的vma结构，vma结构插入mm，表明了合法的用户态虚拟地址空间。
	4. 为各个段分配物理内存空间。（在页表中正确建立虚实映射关系）然后把执行程序各个段的内容拷贝到相应的内核虚拟地址中，至此应用程序执行码和数据已经根据编译时设定地址放置到虚拟内存中了；
	5. 设置用户栈，调用mm_mmap建立用户栈的vma结构
	6. 此时已经建立好vma, mm；将mm->pgdir赋值给cr3（更新用户进程的虚拟内存空间）。
	7. 还需要建立现场。清空进程的中断帧，再重新设置进程的中断帧，使得在执行中断返回指令“iret”后，能够让CPU转到用户态特权级，并回到用户态内存空间，使用用户态的代码段、数据段和堆栈，且能够跳转到用户进程的第一条指令执行，并确保在用户态能够响应中断；`（也就是练习一要做的）`

### practice1

代码按照注释做很简单。

```cpp
static int
load_icode(unsigned char *binary, size_t size) {
    /* LAB5:EXERCISE1 YOUR CODE
     * should set tf_cs,tf_ds,tf_es,tf_ss,tf_esp,tf_eip,tf_eflags
     * NOTICE: If we set trapframe correctly, then the user level process can return to USER MODE from kernel. So
     *          tf_cs should be USER_CS segment (see memlayout.h)
     *          tf_ds=tf_es=tf_ss should be USER_DS segment
     *          tf_esp should be the top addr of user stack (USTACKTOP)
     *          tf_eip should be the entry point of this binary program (elf->e_entry)
     *          tf_eflags should be set to enable computer to produce Interrupt
     */
    tf->tf_cs = USER_CS;
    tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
    tf->tf_esp = USTACKTOP;
    tf->tf_eip = elf->e_entry;
    // #define FL_IF       0x00000200  // Interrupt Flag
    tf->tf_eflags = tf->tf_eflags | FL_IF;  
    ret = 0;
}
```

## 练习二：父进程复制自己的内存空间给子进程（需要编码）

描述：创建子进程的函数do_fork在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过copy_range函数（位于kern/mm/pmm.c中）实现的，请补充copy_range的实现，确保能够正确执行。

### practice2

做了什么：为新的空间创建必要页表（如果没有的话），一个page一个page的做内存复制，并新进程的页表。

```cpp
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)
{
    // ...do{
            /* LAB5:EXERCISE2 YOUR CODE
         * replicate content of page to npage, build the map of phy addr of nage with the linear addr start
         *
         * Some Useful MACROs and DEFINEs, you can use them in below implementation.
         * MACROs or Functions:
         *    page2kva(struct Page *page): return the kernel vritual addr of memory which page managed (SEE pmm.h)
         *    page_insert: build the map of phy addr of an Page with the linear addr la
         *    memcpy: typical memory copy function
         *
         * (1) find src_kvaddr: the kernel virtual address of page
         * (2) find dst_kvaddr: the kernel virtual address of npage
         * (3) memory copy from src_kvaddr to dst_kvaddr, size is PGSIZE
         * (4) build the map of phy addr of  nage with the linear addr start
         */
        // page是根据pte找到的
        // npage是alloc的
        uintptr_t src_kvaddr = page2kva(page);
        uintptr_t dst_kvaddr = page2kva(npage);
        memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
        page_insert(to, npage, start, perm);
    // } while (start != 0 && start < end);
    // ...
}
```

### COW设计

在copy_range时，浅拷贝到页目录表和二级页表，并设置相应page的ref++。在每次写一个页的时候，都先检查ref是否为1。如果非1，则复制一个新的页（改对应页表项），再改新的页。

> 这个想法很简单，可能有缺点。

> 正式版：  
> fork()之后，kernel把父进程中所有的内存页的权限都设为read-only，然后子进程的地址空间指向父进程。当父子进程都只读内存时，相安无事。当其中某个进程写内存时，CPU硬件检测到内存页是read-only的，于是触发页异常中断（page-fault），陷入kernel的一个中断例程。中断例程中，kernel就会把触发的异常的页复制一份，于是父子进程各自持有独立的一份。

> COW缺点：如果在fork()之后，父子进程都还需要继续进行写操作，那么会产生大量的分页错误(页异常中断page-fault)，这样就得不偿失。

## 练习3: 阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现（不需要编码）

### 1. fork/exec/wait/exit 如何影响进程状态？

* fork 创建新的进程状态为 UNINIT

* exec 装载用户程序，进程状态不变

* wait 父进程调用。若无子进程则返回错误，若有子进程 则判定 是否为 ZOMBIE 子进程 有则释放子进程的资源 并返回子进程的返回状态码
若无 ZOMBIE 状态子进程 则进入 SLEEPING 状态 等子进程唤醒。

* exit 清除当前进程几乎所有资源(PCB和内核栈不清除)，将所有子进程(如果有的话)设置为 initproc 进程, 将当前进程状态设置为 ZOMBIE
若有父进程在等待当前进程exit 则 唤醒父进程。此时进程等待父进程调用sys_wait回收。

### 2. 请给出ucore中一个用户态进程的执行状态生命周期图

```
alloc_proc(user_main)            RUNNING
    +                          +--<----<--+
    +                          + proc_run +
    V           schedule()     +-->---->--+ 
PROC_UNINIT -- wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                 +
                                           |      +--- do_exit --> PROC_ZOMBIE                      +
                                           +                                                        + 
                                           -----------------------wakeup_proc------------------------
```

## Challenge: 实现COW

> TODO.


