# s_task 协程库的clion项目

该项目是为了学习s_task这个协程库而创建的，s_task库的地址为 https://github.com/xhawk18/s_task， 使用的版本是 https://github.com/xhawk18/s_task/commit/7b49dc599bb9bc62339ae9db7b46cc94c5bcc26f

## Table of content
- 协程环境初始化
- 协程创建
- 协程的切换

## 协程环境初始化

在main函数的一开始，调用了这么一个宏：
```
__init_async__
```

关于这个宏相关的有这么一些定义：
```c
#   define __await__      __awaiter_dummy__
#   define __async__      s_awaiter_t *__awaiter_dummy__
#   define __init_async__ s_awaiter_t *__awaiter_dummy__ = 0
```

`s_awaiter_t`是一个结构体，暂时撇下不管。`__awaiter_dummy__`这个实际就是一个变量名，和普通的变量`a`, `b`没有区别。调用`__init_async__`实际上就是声明了一个指向`s_awaiter_t`的指针变量，变量名叫做`__awaiter_dummy__`.`__await__`这个宏在后面使用得很多，但实际上只是指向了`__awaiter_dummy__`这个指针。  

接着再来看下函数`s_task_init_system`, 这个函数主要是对`g_globals`这个变量进行初始化，下面来了解下这个`g_globals`这个变量。  

`g_globals`是一个类型为`s_task_globals_t`的结构体  
```c
typedef struct {
    s_task_t    main_task;
    s_list_t    active_tasks;
    s_task_t   *current_task;

#ifndef USE_LIST_TIMER_CONTAINER
    RBTree      timers;
#else
    s_list_t    timers;
#endif

#if defined USE_LIBUV
    uv_loop_t  *uv_loop;
    uv_timer_t  uv_timer;
#endif

#if defined USE_IN_EMBEDDED    
    s_list_t         irq_active_tasks;
    volatile uint8_t irq_actived;
#endif
} s_task_globals_t;
```

函数`s_task_init_system`实际上就只做了这么几件事情：
- 初始化`g_globals`的`active_tasks`, 本质是一个双向链表
- 初始化`g_globals`的`timers`
- 初始化`g_globals`的`main_task`
- 调用了`my_clock_init`

这里出来一个新的数据结构`struct tags_s_task_t`，我们先把这个结构放这里，后面用到的时候再解析。
```c
typedef struct tag_s_task_t {
    s_list_t     node;
    s_event_t    join_event;
    s_task_fn_t  task_entry;
    void        *task_arg;
#if defined   USE_SWAP_CONTEXT
    ucontext_t   uc;
#   ifdef __APPLE__
    char dummy[512]; /* it seems darwin ucontext has no enough memory ? */
#   endif
#elif defined USE_JUMP_FCONTEXT
    fcontext_t   fc;
#endif
    size_t       stack_size;
    bool         waiting_cancelled;
    bool         closed;
} s_task_t;
```

还有这个`my_clock_init`, 这个等到讲时钟子系统的时候再进行讲解。

## 协程创建

在这一节，我们讲解下函数`void s_task_create(void *stack, size_t stack_size, s_task_fn_t task_entry, void *task_arg)`  

`void *stack`应该是指栈的地址,准确地说应该是栈顶的位置(栈在内存中是倒置的)  
`size_t stack_size`看名字应该是栈的大小  
`task_entry`是任务函数的入口地址  
`void *task_arg`是任务的参数  

来看下这个汇编写成的函数
```asm
_make_fcontext:
    /* first arg of make_fcontext() == top of context-stack */
    movq  %rdi, %rax

    /* shift address in RAX to lower 16 byte boundary */
    andq  $-16, %rax

    /* reserve space for context-data on context-stack */
    /* on context-function entry: (RSP -0x8) % 16 == 0 */
    leaq  -0x40(%rax), %rax

    /* third arg of make_fcontext() == address of context-function */
    /* stored in RBX */
    movq  %rdx, 0x28(%rax)

    /* save MMX control- and status-word */
    stmxcsr  (%rax)
    /* save x87 control-word */
    fnstcw   0x4(%rax)

    /* compute abs address of label trampoline */
    leaq  trampoline(%rip), %rcx
    /* save address of trampoline as return-address for context-function */
    /* will be entered after calling jump_fcontext() first time */
    movq  %rcx, 0x38(%rax)

    /* compute abs address of label finish */
    leaq  finish(%rip), %rcx
    /* save address of finish as return-address for context-function */
    /* will be entered after context-function returns */
    movq  %rcx, 0x30(%rax)

    ret /* return pointer to context-data */
```

要读懂这个函数，一行行来读

```asm
 /* first arg of make_fcontext() == top of context-stack */
movq  %rdi, %rax
```

`movq`是数据移动指令，比如将数据可以从寄存器到寄存器、内存到寄存器、从寄存器到内存，但是不能内存到内存。 `q`是后缀，表示操作的是64位数据。  
接着，我们来补充下寄存器的相关知识：
> X86-64有16个64位寄存器，分别是：%rax，%rbx，%rcx，%rdx，%esi，%edi，%rbp，%rsp，%r8，%r9，%r10，%r11，%r12，%r13，%r14，%r15。
> - %rax 作为函数返回值使用。
> - %rsp 栈指针寄存器，指向栈顶
> - %rdi，%rsi，%rdx，%rcx，%r8，%r9 用作函数参数，依次对应第1参数，第2参数
> - %rbx，%rbp，%r12，%r13，%14，%15 用作数据存储，遵循被调用者使用规则，简单说就是随便用，调用子函数之前要备份它，以防他被修改  
> from: https://blog.csdn.net/u013982161/article/details/51347944  

如何在IDEA里面进行汇编语句的调试呢？
![lldb_register_read](./images/lldb_register_read.png) 
那么上面这条指令，就是讲第1参数的值作为返回值  



-16的补码为`fffffff0`，对于负数的补码，就是对齐正数的补码按位取反后+1（这里包括符号位）。  那么这里的`andq  $-16, %rax`这条指令，其实就是`%rax & -16`，实际效果就是最后4位变成0，其余不变。暂时不清楚为什么需要这么做。   
TODO： 未完待续



## 协程的切换
`s_task_join`函数，等待协程执行完，类似于`pthread_join`函数。
```c
int s_task_join(__async__, void *stack) {
    s_task_t *task = (s_task_t *)stack;
    while (!task->closed) {
        int ret = s_event_wait(__await__, &task->join_event);
        if (ret != 0)
            return ret;
    }
    return 0;
}
```

task就代表一个协程，`s_task_join`的主体是一个while循环，直到tasl->closed取值为true的时候或者`s_event_wait`返回值为非0时，退出循环，也就意味着协程task已经执行完成。  

再来看看函数`s_event_wait`
```c
/* Wait event
 *  return 0 on event set
 *  return -1 on event waiting cancelled
 */
int s_event_wait(__async__, s_event_t *event) {
    int ret;
    /* Put current task to the event's waiting list */
    s_list_detach(&g_globals.current_task->node);   /* no need, for safe */
    s_list_attach(&event->wait_list, &g_globals.current_task->node);
    s_task_next(__await__);
    ret = (g_globals.current_task->waiting_cancelled ? -1 : 0);
    g_globals.current_task->waiting_cancelled = false;
    return ret;
}
```
函数`s_event_wait`的入参是一个指向结构体`s_event_t`的指针。结构体`s_event_t`实际上只有一个成员变量`wait_list`，类型是`s_list_t`。这个函数依次做了这么几件事情：
- 将`current_task`的node列表加入到event的wait_list后面，也就是`s_list_attache`函数的作用，将第二个参数的列表加入到第一个参数的列表后面。  
- 调用`s_task_next`函数，调用下一个任务
- 如果current_task的waiting_cancelled是true，那么返回-1，否则返回0
- 将current_task的waiting_cancelled置位false

可能看了这些还是不明白为什么这么设计，暂时先放一边，来看下函数`s_task_next()`函数。这个函数主要实现了协程的切换，看函数名意思是调用下一个task。这个函数主要做了这么几件事情：  
- 将current_task的waiting_cancelled置为false
- 然后是一个while True的循环
    - s_timer_run() 时钟模块的暂时不管
    - 如果`g_globals.active_tasks`不为空的话，调用`s_task_call_next`方法，然后直接return
    - 如果`g_globals.active_tasks`为空的话
        - 如果`!rbt_is_empty(&g_globals.timers)`，也就是g_globals.timers为空的话，调用`my_on_idle(timeout)`
        - 否则调用`my_on_idle((uint64_t)-1)`
        
我们将重点放到函数`s_task_call_next`上。在这个函数内，有两个变量
```c
old_task = g_globals.current_task; // 指向当前的任务，类型是`s_task_t`
next = s_list_get_next(&g_globals.active_tasks); // active_tasks列表的下一个task，但是变量`next`类型是`s_list_t`
```
虽然变量`next`的类型是`s_list_t`，但是下面这行代码，通过计算地址，计算出了变量`next`所处的结构体`s_task_t`的地址，这句话表述不是很清楚。然后把计算出来的`s_task_t`地址赋给了`g_globals.current_task`
```c
g_globals.current_task = GET_PARENT_ADDR(next, s_task_t, node);
```

然后通过`s_list_detach(next)`，把next从列表中删除。最重要的就是下面这段切换的代码(以USE_JUMP_FCONTEXT为例)
```c
        s_jump_t jump;
        jump.from = &old_task->fc;
        jump.to = &g_globals.current_task->fc;
        transfer_t t = jump_fcontext(g_globals.current_task->fc, (void*)&jump);
        s_jump_t* ret = (s_jump_t*)t.data;
        *ret->from = t.fctx;
```

这段代码中，需要明白函数`jump_fcontext`，而这个函数实际上是一段汇编，在我的平台上（macos）通过调试发现这段代码。这段代码看文件头的注释，是从Boost库中找来的
```asm
.text
.globl _jump_fcontext
.align 8
_jump_fcontext:
    leaq  -0x38(%rsp), %rsp /* prepare stack 也就是push了56个字节的数据，因为push指令，就是会将栈顶往小地址移动*/

#if !defined(BOOST_USE_TSX)
    stmxcsr  (%rsp)     /* save MMX control- and status-word */
    fnstcw   0x4(%rsp)  /* save x87 control-word */
#endif

    movq  %r12, 0x8(%rsp)  /* save R12 */
    movq  %r13, 0x10(%rsp)  /* save R13 */
    movq  %r14, 0x18(%rsp)  /* save R14 */
    movq  %r15, 0x20(%rsp)  /* save R15 */
    movq  %rbx, 0x28(%rsp)  /* save RBX */
    movq  %rbp, 0x30(%rsp)  /* save RBP */
    
    /* 上面都在保存上下文 */
    /* store RSP (pointing to context-data) in RAX */
    /* 将栈顶指针保存在寄存器rax中 */
    movq  %rsp, %rax
    
    /* %rdi存储了参数一，也就是g_globals.current_task->fc */
    /* 将rsp栈顶指针移动到了g_globals.current_task->fc */
    /* restore RSP (pointing to context-data) from RDI */
    movq  %rdi, %rsp
    /* R(%rsp)+0x38 指向的内容放入到%r8寄存器 */
    /* 实际上也就是trampoline函数
     * (lldb) register read/x r8
             r8 = 0x000000010cb3bc2c  s_task_test`trampoline
     */
    movq  0x38(%rsp), %r8  /* restore return-address */
    
    /* 接着恢复了一堆寄存器的值
     * %rbx，%rbp，%r12，%r13，%14，%15 用作数据存储，遵循被调用者使用规则，简单说就是随便用，调用子函数之前要备份它，以防他被修改
     */
#if !defined(BOOST_USE_TSX)
    ldmxcsr  (%rsp)     /* restore MMX control- and status-word */
    fldcw    0x4(%rsp)  /* restore x87 control-word */
#endif

    movq  0x8(%rsp), %r12  /* restore R12 */
    movq  0x10(%rsp), %r13  /* restore R13 */
    movq  0x18(%rsp), %r14  /* restore R14 */
    movq  0x20(%rsp), %r15  /* restore R15 */
    movq  0x28(%rsp), %rbx  /* restore RBX */
    movq  0x30(%rsp), %rbp  /* restore RBP */
    /* 将栈顶往栈底方向移动64个字节*/
    leaq  0x40(%rsp), %rsp /* prepare stack */
    
    /* %rdi，%rsi，%rdx，%rcx，%r8，%r9 用作函数参数，
     * 依次对应第1参数，第2参数... */
    
    /* return transfer_t from jump */
    /* RAX == fctx, RDX == data */
    /* 相当于设置参数了,第三个参数设置为(void*)&jump
    movq  %rsi, %rdx
    /* pass transfer_t as first arg in context function */
    /* RDI == fctx, RSI == data */
    /* 第一个参数为%rax，就是准备的栈顶 */
    /* 实际上是作为了s_task_fcontext_entry的参数*/
    movq  %rax, %rdi

    /* indirect jump to context */
    /* 跳转到trampoline
     * 原来jmp不会改变rsp的值，那也就是说，c语言调用的函数调用，rsp值是汇编生成的么？有空得看看
     */
    jmp  *%r8
```

```asm
trampoline:
    /* store return address on stack */
    /* fix stack alignment */
    push %rbp
    /* jump to context-function */
    jmp *%rbx
```

在这段代码执行之前，rsp， rbp, rbx三个寄存器的值如图所示
```text
(lldb) register read/x rsp
     rsp = 0x000000010ccbd0f0  
(lldb) register read/x rbp
     rbp = 0x000000010cb3bc2f  s_task_test`finish
(lldb) register read/x rbx
     rbx = 0x000000010cb39b60  s_task_test`s_task_fcontext_entry at s_task.c:338
```

执行了`push %rbp`之后
```text
(lldb) register read/x rsp
     rsp = 0x000000010ccbd0e8  s_task_test`g_stack_main + 524280
```

再执行`jmp *%rbx`后，就跳转了
```c
void s_task_fcontext_entry(transfer_t arg) {
    /* printf("=== s_task_helper_entry = %p\n", arg.fctx); */

    s_jump_t* jump = (s_jump_t*)arg.data;
    *jump->from = arg.fctx;
    /* printf("%p %p %p\n", jump, jump->to, g_globals.current_task); */

    s_task_context_entry();
}
```

这时候的rsp还是
```text
(lldb) register read/x rsp
     rsp = 0x000000010ccbd0e8  s_task_test`g_stack_main + 524280
```

jump变量还需要再探究下，到底是哪边传过来的。  

接着调用了函数`s_task_fcontext_entry`

```c
void s_task_fcontext_entry(transfer_t arg) {
    /* printf("=== s_task_helper_entry = %p\n", arg.fctx); */

    s_jump_t* jump = (s_jump_t*)arg.data;
    *jump->from = arg.fctx;
    /* printf("%p %p %p\n", jump, jump->to, g_globals.current_task); */

    s_task_context_entry();
}
```
这个函数就做了一件事情，修正jump->from。因为当执行了`movq %rdi, %rsp`以及`jmp`指令以后，更改了栈顶地址和执行目标代码，协程的上下文已经完成切换。`old_task`中的fc是旧协程开始时上下文的栈顶地址，如果要跳回到旧的协程，那么这个栈顶地址显然是过时的，因此通过在`jump_fcontext`记录下准备的上下文栈顶地址，然后通过传参传给`s_task_fcontext_entry`，然后进行更新。  
接着开始调用`s_task_context_entry`  

```c
void s_task_context_entry() {
    struct tag_s_task_t *task = g_globals.current_task;
    s_task_fn_t task_entry = task->task_entry;
    void *task_arg         = task->task_arg;

    __async__ = 0;
    (*task_entry)(__await__, task_arg);

    task->closed = true;
    s_event_set(&task->join_event);
    s_task_next(__await__);
}
```

这个函数主要是调用了task的入口函数，也就是用户函数代码.假设这个用户代码是纯计算代码，不会主动让出cpu，那么当这个函数执行完以后，就会接着执行后面的代码。后面的代码主要做两件事情：
- 将等待这个task完成的那些task加入到队列中
- 协程切换  

协程切换就会切回到main函数的那个栈，进而退出整个程序  

## 时钟子系统
TODO