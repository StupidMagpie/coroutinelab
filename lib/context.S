.global coroutine_entry
coroutine_entry:
    movq %r13, %rdi
    callq *%r12

.global coroutine_switch
coroutine_switch:    

    # [*]直观理解：保存当前的状态到%rdi的context中，恢复%rsi的context
    # TODO: Task 1
    # 保存 callee-saved 寄存器到 %rdi 指向的上下文
    # [*]rdi表示第一个参数
    # 保存的上下文中 rip 指向 ret 指令的地址（.coroutine_ret）
    # [*]通过上一句话，大概可以理清其逻辑，在恢复这个协程的上下文时，会跳转到ret那里，这样自然而然地就顺着yield执行了
    # [*]到这里，我们对yield()有了新的理解，即yield本身可以看作一个白板函数，只不过在执行中间会开小差，随后会接着执行
    # 从 %rsi 指向的上下文恢复 callee-saved 寄存器
    # 最后 jmpq 到上下文保存的 rip
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
    #movq $.coroutine_ret , 120(%rdi)

    movq (%rsi)   ,%rax 
    movq 8(%rsi)  ,%rdi 
    #movq 16(%rsi) ,%rsi 
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
    # 是否需要保存部分寄存器？
    jmpq *120(%rsi)
.global context_ret
context_ret:
.coroutine_ret:
    ret
