#### 苗泽晖，2023010727,计算机31班

## STAGE-1

#### Code

##### context.S

首先是coroutine_switch函数，首先将当前的状态保存到%rdi中，而在调用该函数之前，确保已经将RIP的值指向了context_ret (这是我自己增加的一个函数标签，已经在extern C中注册)，这里标记了下次恢复时的返回地址；接着恢复%rsi的寄存器，跳转到120(%rsi)处

```
.global coroutine_entry
coroutine_entry:
    movq %r13, %rdi
    callq *%r12

.global coroutine_switch
coroutine_switch:    
    # [*]保存当前的状态到%rdi的context中，恢复%rsi的context
    # [*]rdi表示第一个参数
    # [*]在恢复这个协程的上下文时，会跳转到ret那里，这样自然而然地就顺着yield执行了
    # [*]yield本身可以看作一个白板函数，只不过在执行中间会开小差，随后会接着执行
    movq %rax,  (%rdi)
    movq %rdi,  8(%rdi)
    movq %rsi,  16(%rdi)
    movq %rdx,  24(%rdi)
    movq %r8,   32(%rdi)
    movq %r9,   40(%rdi)
    movq %r10,  48(%rdi)
    movq %r11,  56(%rdi)
    movq %rsp,  64(%rdi)
    movq %rbx,  72(%rdi)
    movq %rbp,  80(%rdi)
    movq %r12,  88(%rdi)
    movq %r13,  96(%rdi)
    movq %r14,  104(%rdi)
    movq %r15,  112(%rdi)

    movq (%rsi)   ,%rax 
    movq 8(%rsi)  ,%rdi 
    movq 24(%rsi) ,%rdx 
    movq 32(%rsi) ,%r8  
    movq 40(%rsi) ,%r9  
    movq 48(%rsi) ,%r10 
    movq 56(%rsi) ,%r11 
    movq 64(%rsi) ,%rsp 
    movq 72(%rsi) ,%rbx 
    movq 80(%rsi) ,%rbp 
    movq 88(%rsi) ,%r12 
    movq 96(%rsi) ,%r13 
    movq 104(%rsi),%r14 
    movq 112(%rsi),%r15 
    jmpq *120(%rsi)
.global context_ret
context_ret:
.coroutine_ret:
    ret
```

##### common.h

yield()调用了coroutine_switch函数

```
void yield() {
  if (!g_pool->is_parallel) {
    // 从 g_pool 中获取当前协程状态 [*]在序列化执行模式中，执行完一个后取出下一个
    auto context = g_pool->coroutines[g_pool->context_id];
    // [*]这里的context是正在运行的协程上下文,下一行会将RIP指向context_ret这个函数标签
    context->callee_registers[(int)Registers::RIP] = (uint64_t)context_ret;
    coroutine_switch(context->callee_registers,context->caller_registers);
    // 调用 coroutine_switch 切换到 coroutine_pool 上下文
  }
}
```

##### context.h

resume()函数同理，只不过交换了caller和callee:

```
virtual void resume() {
    caller_registers[(int)Registers::RIP] = (uint64_t)context_ret;
    coroutine_switch(caller_registers,callee_registers);
  }
```

##### coroutine_pool.h

serial_execute_all函数使用循环读取协程池中的context，采用轮询的方式读取未执行完且准备好的协程。当所有都标记为ready后，会结束循环。

```
void serial_execute_all() {
    is_parallel = false;
    g_pool = this;
    int jobNum = coroutines.size();
    int count;
    while(jobNum>0){
      count=0;
      jobNum=0;
      for (auto context : coroutines) {
        // [*]通过一个context执行其协程 -> 查找context.h
        if(!context->finished){
          jobNum++;
          if(context->ready){	//[*]检查是否已经能够执行  
            context_id = count;
            context->resume();	//[*]恢复执行
          }
          else{
            if(context->ready_func()){	//[*]重新检查是否能够执行
              context->ready=true;
              context_id = count;
              context->resume();
            }
          }
        }
        count++;
      }
    }
    for(auto context: coroutines){
      delete context;
    }
    coroutines.clear();
  }
```

#### Question

##### Part-1

```
basic_context(uint64_t stack_size)
      : finished(false), ready(true), stack_size(stack_size) {    //[*]协程的“栈”是虚拟栈，其实是在堆上申请的
    stack = new uint64_t[stack_size];

    // TODO: Task 1
    // 在实验报告中分析以下代码
    // 对齐到 16 字节边界
    uint64_t rsp = (uint64_t)&stack[stack_size - 1];
    rsp = rsp - (rsp & 0xF);

    void coroutine_main(struct basic_context * context);

    callee_registers[(int)Registers::RSP] = rsp;
    // 协程入口是 coroutine_entry
    callee_registers[(int)Registers::RIP] = (uint64_t)coroutine_entry;    // 这里相当于一个label,跳转到开始那里
    // 设置 r12 寄存器为 coroutine_main 的地址
    callee_registers[(int)Registers::R12] = (uint64_t)coroutine_main;
    // 设置 r13 寄存器，用于 coroutine_main 的参数
    // [*]这是执行的那个函数的参数，而不是switch_context的参数
    callee_registers[(int)Registers::R13] = (uint64_t)this;
  }
```

上面是协程上下文的类实现。

- 其成员包含一块栈空间，两个寄存器数组用来保存caller和callee的寄存器情况。

- void coroutine_main(struct basic_context * context);声明了一个函数coroutine_main

- callee_registers[(int)Registers::RSP] = rsp; 设置context的栈指针指向当前的rsp

- callee_registers[(int)Registers::R12] = (uint64_t)coroutine_main; 将R12指向coroutine_main这个函数标签

- callee_registers[(int)Registers::R13] = (uint64_t)this;  将context类本身的指针赋给R13,在稍后会作为coroutine_main的参数

- callee_registers[(int)Registers::RIP] = (uint64_t)coroutine_entry;将context的RIP指向coroutine_entry这个函数标签，而它是在context.S中的一个代码段：

  ```
  .global coroutine_entry
  coroutine_entry:
      movq %r13, %rdi
      callq *%r12
  ```

  在上面我们已经分析过%r13是一个context*，作为函数的参数；%r12则是指向了coroutine_main函数，于是callq就是调用这个函数的过程

##### Part-2

```
// TODO: Task 1
// 在实验报告中分析以下代码
void coroutine_main(struct basic_context *context) {
  context->run();
  context->finished = true;
  context->caller_registers[(int)Registers::RIP] = (uint64_t)context_ret;
  coroutine_switch(context->callee_registers, context->caller_registers); 
  //[*]当正常 run完成之后，标记finished,并且将context切换为调用者

  // unreachable
  assert(false);
}
```

- 首先会使用传入的参数context,执行其中的run()，我们追踪其调用链可以得到：

  ```
  virtual void run() { CALL(f, args); }
  ```

  run()会调用CALL这个宏，有f和args这2个参数。

  ```
  #define CALL(func, args)                                                       \
    CALLER_IMPL(func, 0, args);                                                  \
    CALLER_IMPL(func, 1, args);                                                  \
    CALLER_IMPL(func, 2, args);                                                  \
    CALLER_IMPL(func, 3, args);                                                  \
    CALLER_IMPL(func, 4, args);                                                  \
    CALLER_IMPL(func, 5, args);                                                  \
    CALLER_IMPL(func, 6, args);                                                  \
    CALLER_IMPL(func, 7, args);
  ```

  其又会调用CALLER_IMPL,继续追踪：

  ```
  #define CALLER_IMPL(func, x, args)                                             \
    if constexpr (std::tuple_size_v<std::decay_t<decltype(args)>> == x)          \
    func(EXPAND_CALL_##x(args))
  ```

  这个宏会最终产生一条函数调用。

- context->finished = true; 将协程标记为“执行完毕”，这会在协程池的调度过程中起作用，让我们能够直接跳过执行完毕的协程

-   context->caller_registers[(int)Registers::RIP] = (uint64_t)context_ret;
    coroutine_switch(context->callee_registers, context->caller_registers); 

  这两条就是执行完毕，返回调度器的过程，也就是切换回coroutine_pool的上下文。之于RIP的赋值我们已经分析过

#### Optional



## STAGE-2

#### Code

完成了sleep函数：

```
void sleep(uint64_t ms) {
  auto cur = get_time();
  if (g_pool->is_parallel) {    // [*]对于并发执行，并不需要主动阻塞，一直执行完后线程会自动汇合
    while (
        std::chrono::duration_cast<std::chrono::milliseconds>(get_time() - cur)
            .count() < ms)
      ;
  } 
  else {
    // 从 g_pool 中获取当前协程状态
    // 获取当前时间，更新 ready_func
    // ready_func：检查当前时间，如果已经超时，则返回 true
    // 调用 coroutine_switch 切换到 coroutine_pool 上下文
    auto context = g_pool->coroutines[g_pool->context_id];
    context->ready=false;
    context->ready_func= [&cur,&ms](){    //[*]使用lambda表达式，其中[]为捕获列表，将外界的一部分变量捕获，这样就能在lambda内部使用
      return (std::chrono::duration_cast<std::chrono::milliseconds>(get_time() - cur).count() >= ms);
    };
    yield();
  }
}
```

在sleep开始的时候，记录开始的时刻。然后将 context->ready标记为false,代表协程处于阻塞状态，暂时无法执行。

接着注册一个ready_func函数，使用lambda表达式，捕获当前环境中的cur和ms，用来产生正确的判断函数。

```
if(!context->finished){
  jobNum++;
  if(context->ready){
    //[*]每一个context都是一个指针
    context_id = count;
    context->resume();
    // [*]是否需要从协程池中删除这个context?
  }
  else{
    if(context->ready_func()){
      context->ready=true;
      context_id = count;
      context->resume();
    }
  }
}
```

在协程池中，我们判断一个context是否能够执行，如果不能，就调用ready_func来检查，如果这时候发现睡眠时间已经满足要求，可以标记为ready并执行



## STAGE-3

#### Code

```
void lookup_coroutine(const uint32_t *table, size_t size, uint32_t value,
                      uint32_t *result) {
  size_t low = 0;
  while ((size / 2) > 0) {
    size_t half = size / 2;
    size_t probe = low + half;
    /***************/
    __builtin_prefetch(&table[probe],0,1);
    yield();
	/**************/
    uint32_t v = table[probe];
    if (v <= value) {
      low = probe;
    }
    size -= half;
  }
  *result = low;
}
```

缓存易失的部分是table[probe]，我们通过__builtin_prefetch的方法尝试提前获取，然后切换到下一个协程。在重新切换回这个协程的过程中，prefetch过程执行完毕，因此能够提升性能。

#### Performance

##### 默认执行

```
Size: 4294967296
Loops: 1000000
Batch size: 16
Initialization done
naive: 1596.70 ns per search, 49.90 ns per access
coroutine batched: 1300.83 ns per search, 40.65 ns per access
```

提升了大约18%

##### 比较不同的batch

```
2023010727@ics24:~/coroutinelab/bin$ ./binary_search -b 2
Size: 4294967296
Loops: 1000000
Batch size: 2
Initialization done
naive: 1585.78 ns per search, 49.56 ns per access
coroutine batched: 1674.83 ns per search, 52.34 ns per access

2023010727@ics24:~/coroutinelab/bin$ ./binary_search -b 4
Size: 4294967296
Loops: 1000000
Batch size: 4
Initialization done
naive: 1469.69 ns per search, 45.93 ns per access
coroutine batched: 1273.15 ns per search, 39.79 ns per access

2023010727@ics24:~/coroutinelab/bin$ ./binary_search -b 8
Size: 4294967296
Loops: 1000000
Batch size: 8
Initialization done
naive: 1637.34 ns per search, 51.17 ns per access
coroutine batched: 1282.29 ns per search, 40.07 ns per access

2023010727@ics24:~/coroutinelab/bin$ ./binary_search -b 16
Size: 4294967296
Loops: 1000000
Batch size: 16
Initialization done
naive: 1540.61 ns per search, 48.14 ns per access
coroutine batched: 1303.06 ns per search, 40.72 ns per access

2023010727@ics24:~/coroutinelab/bin$ ./binary_search -b 32
Size: 4294967296
Loops: 1000000
Batch size: 32
Initialization done
naive: 1462.55 ns per search, 45.70 ns per access
coroutine batched: 1966.23 ns per search, 61.44 ns per access

```

可以看到当batch过小时和过大时，使用协程的效率都会降低。前者可能是因为在轮询的过程中来不及完成prefetch过程；后者可能是因为一组协程过多，导致轮询一周中的时间利用率低，此时协程之间的切换浪费时间所占比例又增多，表现为整体效率低下。

##### 比较不同的size

```
2023010727@ics24:~/coroutinelab/bin$ ./binary_search -l 4
Size: 16
Loops: 1000000
Batch size: 16
Initialization done
naive: 5.16 ns per search, 1.29 ns per access
coroutine batched: 216.92 ns per search, 54.23 ns per access

2023010727@ics24:~/coroutinelab/bin$ ./binary_search -l 16
Size: 65536
Loops: 1000000
Batch size: 16
Initialization done
naive: 33.09 ns per search, 2.07 ns per access
coroutine batched: 568.67 ns per search, 35.54 ns per access

2023010727@ics24:~/coroutinelab/bin$ ./binary_search -l 24
Size: 16777216
Loops: 1000000
Batch size: 16
Initialization done
naive: 212.37 ns per search, 8.85 ns per access
coroutine batched: 822.02 ns per search, 34.25 ns per access

2023010727@ics24:~/coroutinelab/bin$ ./binary_search -l 28
Size: 268435456
Loops: 1000000
Batch size: 16
Initialization done
naive: 751.84 ns per search, 26.85 ns per access
coroutine batched: 971.47 ns per search, 34.70 ns per access

2023010727@ics24:~/coroutinelab/bin$ ./binary_search -l 32
Size: 4294967296
Loops: 1000000
Batch size: 16
Initialization done
naive: 1510.65 ns per search, 47.21 ns per access
coroutine batched: 1264.62 ns per search, 39.52 ns per access

```

我们可以发现，当处理的块较小时，朴素的方法总是更快；随着处理的块增大，协程方法效率会逐渐提升，最后优于朴素方法。这可能是因为对于越大的块，发生缓存丢失的可能性增大，使得朴素方法变慢，而协程所受影响则几乎不变。

