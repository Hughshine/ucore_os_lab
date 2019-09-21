# lab0 的一些记录

> [hughshine](https://hughthin.github.io) at github
>
> 是记录在onenote上的，为了显示我做了很多预备内容（因为自己什么都不会），复制过来。
>
> 没有整理格式，还请谅解。

## techniques

```
为 rm 加上 alias: alias rm="rm -i"
make 编译源码
make grade 测试是否基本正确 
```

## First: some useful linux( ubuntu ) features: 

**重定向：** 
```
command >>      filename #接在末尾
ls > test.tx t覆盖 
```

**管道：**
```
ls -l | less 
```

**后台进程：**

```
【cli不是串行的】sleep 10 & ls ; 

& 与 ; 的区别可以就理解在同步/异步启动进程的层面；

<ctrl-z> => 暂停某个进程

bg 使之转入后台，fg使之转回前台；【如果有好多个任务在后台会如何？】【这里任务是以进程为单位的】

ps: processes status

```

**软件包管理（ubuntu）：**

```
apt-get 适用于      deb包管理式的操作系统

sudo apt-get install <package>

sudo apt-get remove <package>

apt-cache search <pattern>

apt-cache show <package>

- 配置升级源

ubuntu软件包获取依赖升级源，可以以root权限修改 /etc/apt/sources,list【或者更新包管理器】

```

 

man <command> 

 

diff & patch

 

deb 遵循严格的依赖关系（于 Depends 和 Pre-Depends 指定）

 

printf - formatted output

 

vim 的配置文件位置：\.vimrc

 

- 有关汇编：

- GNU汇编（assembler）采用的是AT&T汇编格式（gcc），Microsoft  汇编采用Intel格式（VC++）。

- AT&T汇编基本语法：

- 1. 寄存器前加“%”
  2. 左边是源操作数，右边是目的操作数。常数、立即数加“$”
  3. 操作数长度标识？【word byte 等       体现在指令名中】
  4. 寻址方式 【immed32(basepointer,       indexpointer, indexscale)】

- pmode  保护模式 【还】

- 基址寻址，变址寻址（变址寄存器（首地址） + 指令地址码（位移量））？？？

- 内联汇编:

- - asm(); or       __asm__();
  - 多行汇编时，不要忘记控制字符

- 拓展内联汇编【了解的很粗略】：

- - %num，表示使用寄存器的样板操作数
  - 则使用寄存器需要两个“%”

 

<https://www.cnblogs.com/hdk1993/p/4820353.html>

 

AT&T就是那种，带一堆百分号的那种。

上面这个链接里有Helloworld例程。以及和汇编相关的内容。

 

GAS ==> GNU Assembler; GNU 是“计划”

 

<http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html#s7>

how to use inline assembly in gcc

  

- GNU  make, makefile 

- 

- makefile 的简单规则：

- 

- target ... :  prerequisites  ...

- command

- ...

- ... 

- 意思是，生成的目标文件 x，它的依赖是 prerequisites，每当有的依赖比目标文件新了，它就执行command。

- 

- 达成的效果大致为：

- - 如果这个工程没有编译过，那么我们的所有c文件都要编译并被链接。
  - 如果这个工程的某几个c文件被修改，那么我们只编译被修改的c文件，并链接目标程序。
  - 如果这个工程的头文件被改变了，那么我们需要编译引用了这几个头文件的c文件，并链接目标程序。

- 

 

gdb使用：GNU软件系统中的标准侦错器。

- 调试前，必须使用-g /      -ggdb 编译源文件。
- gdb      bugging【加载可执行文件】

在gdb中运行：

- run【开始跑】
- where【哪里错了】
- list【查看出错附近的代码】
- break      num 【在某行设置断点】
- next 单步执行
- print      string【使用print命令输出某个变量】

可视化窗口查看源代码：

(gdb) layout src 

&

ctrl-X + A

&

gdb -tui

 

<https://chyyuu.gitbooks.io/ucore_os_docs/content/lab0/lab0_2_3_3_gdb.html>

 

对于里面的gdb调试实例，gets()需要被换【被stdio抛弃】，fgets(string , sizepf(string), stdin)

改了之后，出错点似乎变成了库中的函数，不在主函数中了

 

- 进一步学习内容：

- gcc tools相关文档

-  

- 【ubuntu的实验代码push 不上去【已解决】】[fetch pull 的区别？]

 

\## 基于硬件模拟器的源码级调试

qemu 可以加载镜像文件，并设定以视为什么镜像、作为哪块硬盘的镜像、外接设备设置、虚存大小等等

 

qemu 能做的事：跟断点有关的事；跟内存有关的事【输出到文件中等】；跟寄存器有关的事；info【似乎mac版qemu的指令不是很一样】【ubuntu还没试过】

 

- qemu无法在哪里都可以运行，但是已经安装好了。ubuntu 上还没好用。

- 【mac上似乎好用了】

-  

- wget 怎么报错了【重装了一下，好了。【library not loaded】

 

gdb 结合 qemu做调试

 

1. 先编译，在lab文件夹下，
2. qemu -S -s -hda      ./bin/ucore.img -monitor stdio
3. 开启新的终端，target remote 127.0.0.1:1234
4. 可以调试了。设置break point      调试

 

通过gdb可以对ucore代码进行调试，以lab1中调试memset函数为例：

(1) 运行 qemu -S -s -hda ./bin/ucore.img -monitor stdio

(2) 运行 gdb并与qemu进行连接

(3) 设置断点并执行

(4) qemu 单步调试。

 

 

因为很麻烦，所以可以写成脚本。

【注意可能需要给可执行权限 chmod 744 filename】

 

远程连接后，手动载入符号表等调试信息

(gdb) add-symbol-file android_test/system/bin/linker 0x6fee6180

 

设定调试目标架构：

(gdb) set arch i8086

 

内核对于操作系统【它俩究竟是啥关系？】：

现代[操作系统](https://baike.baidu.com/item/%E6%93%8D%E4%BD%9C%E7%B3%BB%E7%BB%9F)设计中，为减少系统本身的开销，往往将一些与[硬件](https://baike.baidu.com/item/%E7%A1%AC%E4%BB%B6)紧密相关的（如中断处理程序、[设备驱动程序](https://baike.baidu.com/item/%E8%AE%BE%E5%A4%87%E9%A9%B1%E5%8A%A8%E7%A8%8B%E5%BA%8F)等）、基本的、公共的、运行频率较高的模块（如时钟管理、[进程](https://baike.baidu.com/item/%E8%BF%9B%E7%A8%8B)调度等）以及[关键](https://baike.baidu.com/item/%E5%85%B3%E9%94%AE)性数据结构独立开来，使之常驻内存，并对他们进行保护。通常把这一部分称之为操作系统的[内核](https://baike.baidu.com/item/%E5%86%85%E6%A0%B8)。

 

内核是计算机的一些核心内容，与硬件关系更紧密、对于操作系统更根本的、运行频率较高的（时钟？），关键数据结构，等内容，使之常驻内存，对其保护。它们被称为内核。

一些辅助性的程序被抛开在内核之外。

 

为什么运行可执行文件要加 './'：

shell会在环境变量包含路径中去找文件。unix/linux因为安全考虑没有将当前路径放入$PATH中.

 

gdb单独调试的时候，一般是一个加了 -g 的符号文件。我们希望调试一个镜像，镜像需要承载在qemu上；这里需要gdb远程连接qemu，并手动加载符号表（img可能没有包含调试信息？这里不清楚，但不是很重要）。

 

“由于分配地址的动态性，gdb并不知道这个分配的地址是多少，因此当我们在对这样动态链接的代码进行调试的时候，需要手动要求gdb将调试信息加载到指定地址。”

 

- 了解处理器硬件

-  

- 了解ucore，需要出息其硬件环境，即：处理器体系结构（Intel 80386以上的）与机器指令集。

-  

- Intel 80386的运行模式：（四种）实模式、保护模式、SMM模式和虚拟8086模式。

- 实模式：（8086，16位，随意可访问内存空间不超过1M），只是为了向下兼容。操作系统和用户程序没有区别。程序代码和数据位于不同的区域（现在的是啥样的？）。

- 保护模式：（80386首先在实模式下初始化控制寄存器（哪些？）以及页表。然后再通过设置保护模式使能位进入保护模式）。

- - 保护模式中，32位均可寻址。支持内存分页机制（虚拟内存良好支持），支持多任务，知识优先级机制（特权级，0~3）。

-  

- Intel 80386的内存架构：

- - 逻辑地址是应用程序看到的地址。

- - 线性地址空间是80386处理器通过段（Segment）机制控制下的形成的地址空间。

- - 段机制：在操作系统的管理下，每个运行的应用程序有相对独立的一个或多个内存空间段，每个段有各自的起始地址和长度属性，大小不固定，这样可让多个运行的应用程序之间相互隔离，实现对地址空间的保护。
  - 在操作系统完成对80386处理器页机制的初始化和配置（主要是需要操作系统通过特定的指令和操作建立页表，完成虚拟地址与线性地址的映射关系）

-  

- - 分段机制启动、分页机制未启动：逻辑地址--->段机制处理--->线性地址=物理地址
  - 分段机制和分页机制都启动：逻辑地址--->段机制处理--->线性地址--->页机制处理--->物理地址

-  

-  

- Intel 80386寄存器：

- 一共八组，一般程序员可见四组：
     通用寄存器，段寄存器，指令指针寄存器，标志寄存器，系统地址寄存器，控制寄存器，调试寄存器，测试寄存器

-  

- 通用寄存器的低16位对应8086的对应寄存器。（起名加上了‘e’应是为与8086寄存器区分开）

-  

- 段寄存器（6个，其中两个附加段）：code segment，data segment，extra segment（附加数据段），stack segment，fs，gs

-  

- 指令指针寄存器IP

-  

- 标志寄存器Flag Register：

- 直接复制了。

- CF(Carry  Flag)：进位标志位；
  ​       PF(Parity Flag)：奇偶标志位；
  ​       AF(Assistant Flag)：辅助进位标志位；
  ​       ZF(Zero Flag)：零标志位；
  ​       SF(Singal Flag)：符号标志位；
  ​       IF(Interrupt  Flag)：中断允许标志位,由CLI，STI两条指令来控制；设置IF位使CPU可识别外部（可屏蔽）中断请求，复位IF位则禁止中断，IF位对不可屏蔽外部中断和故障中断的识别没有任何作用；
  ​       DF(Direction  Flag)：向量标志位，由CLD，STD两条指令来控制；
  ​       OF(Overflow Flag)：溢出标志位；
  ​       IOPL(I/O Privilege  Level)：I/O特权级字段，它的宽度为2位,它指定了I/O指令的特权级。如果当前的特权级别在数值上小于或等于IOPL，那么I/O指令可执行。否则，将发生一个保护性故障中断；
  ​       NT(Nested  Task)：控制中断返回指令IRET，它宽度为1位。若NT=0，则用堆栈中保存的值恢复EFLAGS，CS和EIP从而实现中断返回；若NT=1，则通过任务切换实现中断返回。在ucore中，设置NT为0。

-  

-  

- 其他应用程序无法访问的寄存器，如CR0，CR2，CR3...

-  

 

- 页机制和段机制有一定程度的功能重复，但Intel公司为了向下兼容等目标，使得这两者一直共存。

-  

- 没有很理解，段机制、页机制，究竟是cpu做的还是操作系统做的？【得把计组书拿出来了】

 

- ucore编程方法和通用数据结构：

- - 面向对象编程（类似接口）（在C中表现为一组函数指针的集合）（难点是找到“各种内核子系统的共性访问/操作模式”，以提取函数指针列表）
  - 通用数据结构：双向循环列表

- 因为大量使用双向循环列表，而结点所保存的数据各异，在不希望重复定义链表操作的想法上，以如下方式设计该数据结构。

- - 定义不带data部分的双向链表，在每次要定义新的链表时，使之成为新结点（宿主数据结构）的一个属性。（这个节点，包含基本双向链表节点和指向其数据的指针（以及一些其他内容））。

- 在链表上定义初始化插入删除的操作（inline）；

- - 此时出现一个问题，就是无法访问宿主数据结构。linux定义了一个le2XXX(le,       member)的宏，用来做这个转换。它实际上是通过一些方式计算出来的地址。。

- le 是 list entry，member是XXX数据类型中包含的“链表节点的成员变量”。【这个地方没有很看懂，不知道到底要传啥，就是member究竟是啥】

- 意思是：先求得数据结构的成员变量在本宿主数据结构中的偏移量，然后根据成员变量的地址反过来得出属主数据结构的变量的地址。

 

c中struct只能定义数据成员，不能定义成员函数。【定义函数指针】

 

删除的时候，要重新初始化被删除节点的前向与后向指针，保证其无法在访问表中数据结构。

 

elf - executable and linkable format

readelf 分析elf格式 的可执行文件

dd：读写数据到文件和设备中的工具

ld：链接器

nm：查看执行文件中的变量、函数的地址

 

一些参考资料。。怎么看呀。。。。。。。。。。算了。。