# lab6

Table of Contents
=================

   * [lab6](#lab6)
      * [notes](#notes)
         * [抢占点](#抢占点)
         * [进程切换](#进程切换)
      * [practices](#practices)
         * [practice0](#practice0)
         * [practice1 使用 Round Robin 调度算法](#practice1-使用-round-robin-调度算法)
         * [practice2  实现 Stride Scheduling 调度算法（需要编码）](#practice2--实现-stride-scheduling-调度算法需要编码)
            * [stride-scheduling 算法（SS算法）](#stride-scheduling-算法ss算法)
               * [溢出问题](#溢出问题)
            * [实现](#实现)
      * [make grade](#make-grade)

## notes

### 抢占点

ucore 在用户态，是可抢占的；在内核态是不可抢占的，只会主动放弃cpu控制权。

可抢占体现在，在用户态被中断，如果进程need_reshed，则放弃cpu控制权。

```cpp
// trap.c
if (!in_kernel) {
    //……
    if (current->need_resched) {
        schedule();
    }
}
```

> “如果没有第一行的if语句，那么就可以体现对内核代码的可抢占性。但如果要把这一行if语句去掉，我们就不得不实现对ucore中的所有全局变量的互斥访问操作，以防止所谓的racecondition现象，这样ucore的实现复杂度会增加不少。”

### 进程切换

1. 中断/syscall，进入内核态
2. trap(), 先trap_dispatch()执行对应中断
3. 执行结束后，根据是否need_reshed，schedule()。此时trapframe是被保存的。
4. 再次被调用，首先proc_run()恢复现场。开始正常执行（从switch_to下一句），trapret，iret，回归之前的用户态。

## practices

### practice0

为了兼容mac，修改grade脚本第264行为以下，原因在[这里](https://serverfault.com/questions/501230/can-not-seem-to-get-expr-substr-to-work)。

```shell
select=`echo $1 | cut -c 2-${#1}`
```

更新代码，需要修改的地方有两处：

```cpp
static struct proc_struct *alloc_proc(void) {
    // 为了适应更复杂的调度器功能，进程有了新的几个成员，需要初始化
    proc->rq = NULL;
    list_init(&(proc->run_link));
    proc->time_slice = 0;
}
```

```cpp
/**
void
sched_class_proc_tick(struct proc_struct *proc) {
    if (proc != idleproc) {
        sched_class->proc_tick(rq, proc);
    }
    else {
        proc->need_resched = 1;
    }
}**/
static void trap_dispatch(struct trapframe *tf) {
    //...
    // 在每个timetick，使用调度器的属性调整函数，更新当前执行进程的状态
    ++ticks;
    sched_class_proc_tick(current);
    // current->need_resched = 1;
    //...
}
```

### practice1 使用 Round Robin 调度算法

1. 请理解并分析sched_class中各个函数指针的用法，并结合Round Robin 调度算法描ucore的调度执行过程。

调度器被抽象出四个函数：运行队列的增、删、选择，状态修改（根据相应调度算法、一般是对正在运行的进程的某些状态做改动，并进一步判断是否到达调度时机）。

对于RR算法，只需要关注它关于这四个函数的具体实现。RR算法在每个时间片结束后，按照FCFS选择下一个运行进程。

增：

```cpp
//将进程（一般是刚运行完一个时间片的进程）放入末尾，并将该进程时间片长度恢复最大值
static void
RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));
    list_add_before(&(rq->run_list), &(proc->run_link));
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    proc->rq = rq;
    rq->proc_num ++;
}
```

删：

```cpp
// 将这个进程从队列里删除。删除方法是：修改前后节点的指针，并且将自己的前后向指针都指向自己。
static void
FCFS_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link));
    rq->proc_num --;
}
```

选：

```cpp
//就是选择队列里的第一个即可，利用宏把指针弄成相应的proc块
static struct proc_struct *
FCFS_pick_next(struct run_queue *rq) {
    list_entry_t *le = list_next(&(rq->run_list));
    if (le != &(rq->run_list)) {
        return le2proc(le, run_link);
    }
    return NULL;
}
```

修改状态：

```cpp
// RR算法中，在每个tick，减少当前进程的time_slice
// 如果已经为0，则标识它可以被rescheduled，在中断处理后期会调用schedule()，把它换掉。
static void
RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (proc->time_slice > 0) {
        proc->time_slice --;
    }
    if (proc->time_slice == 0) {
        proc->need_resched = 1;
    }
}
```

2. 请在实验报告中简要说明如何设计实现”多级反馈队列调度算法“，给出概要设计，鼓励给出详细设计。

多级反馈队列算法，将运行队列分为多个，每个队列优先级不同。高优先级队列时间片短，但先被schedule。一个proc最开始在最高优先级队列中，每次schedule后，若没有执行完，放入下一级队列，直到放入最低优先级队列。（每个队列内部是FCFS的）。

设计：数据结构，需要优先级条队列，设为N。

增：新的进程放入最高优先级队列中，初始化timeslice。

删：从当前队列直接删掉就好了。

选：从最高优先级队列开始，依次向下查，找到第一个非空队列里的第一个。

状态修改：timetick，降低当前进程的timeslice；如果为0了，还没结束，放入比之前第一个优先级的队列中，并schedule。

### practice2  实现 Stride Scheduling 调度算法（需要编码）

#### stride-scheduling 算法（SS算法）

SS算法引入了简单的优先级机制。在最静态的情况下，它为每个proc增加三个属性，`tickets`, `stride`, `pass`，tickets是权重，stride是这个进程两次调度等候间隔的时长（单位是quantum，一个定义的常数，可以理解为时间片长度），pass为这个进程下一次被调度的时间点。stride与tickets的倒数呈负相关，实现时不使用浮点数，转而用大整数触发近似代替，如此便可以很好的适应时间片离散的性质（作为调度时间的基本单位）。

$$
stride_i = \frac{stride_1}{tickets} 
$$

在每个时间调度点，选择pass最小的进程替换当前进程，更新当前进程的pass值并放入（优先）队列。（当然，如果还是自己，只做参数更新就可以了。）

> ucore上给的介绍，参数定义和论文不相符。

以上叙述的是最简单的情况，实际运行中，进程数量、优先级都可能会变，需要不断对参数进行动态调整。例如在进程变多时，我们希望每个进程每一次调度的运行时间更短，以保持交互性。在进程运行更久时，我们可能希望降低进程的优先级。

> 不过这些都没有在实验指导书中说明。

##### 溢出问题

因为pass是不断增长的，最后很有可能会溢出，导致比较问题。我们可以使用特殊的比较方法解决它。将pass设置为unsigned，将最大步长设置为有符号整数最大值，我们就可以利用pass的有符号比较，得出正确结果。

```cpp
(int32_t)(p->lab6_stride - q->lab6_stride) > 0
```

#### 实现

为proc增加一个新的属性`uint32_t lab6_pass`，初始化proc时，设置相应属性。
```cpp
    proc->lab6_run_pool.parent = proc->lab6_run_pool.left = proc->lab6_run_pool.right = NULL;
    // 优先级 (和步进成反比)
    proc->lab6_priority = 0;
    // 步进值
    proc->lab6_stride = 0;
    proc->lab6_pass = 0;
```

按照注释填写SS的各个函数。

```cpp
// 为避免溢出问题，设置BIG_STRIDE为有符号int的上限
#define BIG_STRIDE (((uint32_t)-1) / 2)

/* The compare function for two skew_heap_node_t's and the
 * corresponding procs*/
// 为了和论文命名同步，比较的时候我希望按pass比较
static int
proc_stride_comp_f(void *a, void *b)
{
     struct proc_struct *p = le2proc(a, lab6_run_pool);
     struct proc_struct *q = le2proc(b, lab6_run_pool);
     // int32_t c = p->lab6_stride - q->lab6_stride;
     int32_t c = p->lab6_pass - q->lab6_pass;

     if (c > 0) return 1;
     else if (c == 0) return 0;
     else return -1;
}
// 初始化时，init 运行队列（没有很大作用），和斜堆
static void
stride_init(struct run_queue *rq) {
     /* LAB6: YOUR CODE 
      * (1) init the ready process list: rq->run_list
      * (2) init the run pool: rq->lab6_run_pool
      * (3) set number of process: rq->proc_num to 0       
      */
     list_init(&rq->run_list);
     rq->lab6_run_pool = NULL;
     rq->proc_num = 0;
}

static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: YOUR CODE 
      * (1) insert the proc into rq correctly
      * NOTICE: you can use skew_heap or list. Important functions
      *         skew_heap_insert: insert a entry into skew_heap
      *         list_add_before: insert  a entry into the last of list   
      * (2) recalculate proc->time_slice
      * (3) set proc->rq pointer to rq
      * (4) increase rq->proc_num
      */
     // 对斜堆的操作，每次都返回新的根，因而重新赋值。
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
     if (proc->lab6_priority == 0)
     {
          proc->lab6_priority = 1;
     }
    // 重置time slice
     if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice)
     {
          proc->time_slice = rq->max_time_slice; // max_time_slice ==> quantum
     }
     proc->rq = rq;
     rq->proc_num += 1;
}

/*
 * stride_dequeue removes the process ``proc'' from the run-queue
 * ``rq'', the operation would be finished by the skew_heap_remove
 * operations. Remember to update the ``rq'' structure.
 *
 * hint: see libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    // 相似
     /* LAB6: YOUR CODE 
      * (1) remove the proc from rq correctly
      * NOTICE: you can use skew_heap or list. Important functions
      *         skew_heap_remove: remove a entry from skew_heap
      *         list_del_init: remove a entry from the  list
      */
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
     --rq->proc_num;
}

static struct proc_struct *
stride_pick_next(struct run_queue *rq) {
     /* LAB6: YOUR CODE 
      * (1) get a  proc_struct pointer p  with the minimum value of stride
             (1.1) If using skew_heap, we can use le2proc get the p from rq->lab6_run_poll
             (1.2) If using list, we have to search list to find the p with minimum stride value
      * (2) update p;s stride value: p->lab6_stride
      * (3) return p
      */
     if (rq->lab6_run_pool == NULL)
     {
          return NULL;
     }
     // 斜堆的顶就是 pass 值最小的进程
     struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
    // 被选择后，pass更新为下一个。其实开始的时候算出来stride，直接+=stride即可。
     p->lab6_pass += BIG_STRIDE / p->lab6_priority;
     return p;
}

static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    // 因为SS算法也是基于时钟中断的，时间片长度也是固定的（quantum），此处和RR算法一样。
     /* LAB6: YOUR CODE */
     if (proc->time_slice == 0)
     {
          proc->need_resched = 1;
     }
     else
     {
          --proc->time_slice;
     }
}

```

## `make grade`

目前没有通过make grade，可能是复制代码的时候复制的不对。