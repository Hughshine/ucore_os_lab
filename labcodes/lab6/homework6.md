# lab6

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

### practice2


