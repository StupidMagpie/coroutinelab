#pragma once
#include <assert.h>
#include <cstdint>
#include <functional>
#include <tuple>
#include <type_traits>

enum class Registers : int {
  RAX = 0,
  RDI,
  RSI,
  RDX,
  R8,
  R9,
  R10,
  R11,
  RSP,
  RBX,
  RBP,
  R12,
  R13,
  R14,
  R15,
  RIP,
  RegisterCount
};
// 这里用到了context.S中的部分
extern "C" {
void coroutine_entry();
void coroutine_switch(uint64_t *save, uint64_t *restore);
void context_ret();
}

struct basic_context {
  uint64_t *stack;
  uint64_t stack_size;
  uint64_t caller_registers[(int)Registers::RegisterCount];
  uint64_t callee_registers[(int)Registers::RegisterCount];
  bool finished;
  bool ready;
  std::function<bool()> ready_func;

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

  ~basic_context() { delete[] stack; }

  virtual void run() = 0;
  virtual void resume() = 0;
};

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

extern __thread basic_context *g_current_context;

// boilerplate code to handle variadic function arguments
#define EXPAND_CALL_0(args)
#define EXPAND_CALL_1(args) (std::get<0>(args))
#define EXPAND_CALL_2(args) EXPAND_CALL_1(args), (std::get<1>(args))
#define EXPAND_CALL_3(args) EXPAND_CALL_2(args), (std::get<2>(args))
#define EXPAND_CALL_4(args) EXPAND_CALL_3(args), (std::get<3>(args))
#define EXPAND_CALL_5(args) EXPAND_CALL_4(args), (std::get<4>(args))
#define EXPAND_CALL_6(args) EXPAND_CALL_5(args), (std::get<5>(args))
#define EXPAND_CALL_7(args) EXPAND_CALL_6(args), (std::get<6>(args))

#define CALLER_IMPL(func, x, args)                                             \
  if constexpr (std::tuple_size_v<std::decay_t<decltype(args)>> == x)          \
  func(EXPAND_CALL_##x(args))

#define CALL(func, args)                                                       \
  CALLER_IMPL(func, 0, args);                                                  \
  CALLER_IMPL(func, 1, args);                                                  \
  CALLER_IMPL(func, 2, args);                                                  \
  CALLER_IMPL(func, 3, args);                                                  \
  CALLER_IMPL(func, 4, args);                                                  \
  CALLER_IMPL(func, 5, args);                                                  \
  CALLER_IMPL(func, 6, args);                                                  \
  CALLER_IMPL(func, 7, args);

/**
 * @brief
 * 协程运行时资源管理。存储了协程函数，以及协程函数的运行时栈即寄存器内容等。
 *
 * @tparam F 协程函数类
 * @tparam Args 协程函数所需要的参数列表    __builtin_prefetch(&table[probe]);

 * 在当前情况下，协程函数支支持展开至多 7 个参数。
 * 如果需要更多的参数需要
 *   1. 参考修改 CALL 的宏定义以及添加对应的 EXPAND_CALL_X 的宏定义。
 *   2. 需要修改构造函数中的 static_assert。
 */
template <typename F, typename... Args>
struct coroutine_context : public basic_context {
  F f;
  std::tuple<Args...> args;

  // construct a stacked coroutine, with stack size 16 KB
  coroutine_context(F f, Args... args)
      : f(f), args(std::tuple<Args...>(args...)),
        basic_context(16 * 1024 / sizeof(uint64_t)) {
    static_assert(sizeof...(args) <= 7);
  }

  // construct a stacked coroutine, with stack_size (in KB)
  coroutine_context(uint64_t stack_size, F f, Args... args)
      : f(f), args(std::tuple<Args...>(args...)),
        basic_context(stack_size * 1024 / sizeof(uint64_t)) {
    static_assert(sizeof...(args) <= 7);
  }

  /**
   * @brief 恢复协程函数运行。
   * TODO: Task 1
   * 你需要保存 callee-saved 寄存器，并且设置协程函数栈帧，然后将 rip 恢复到协程
   * yield 之后所需要执行的指令地址
   */
  virtual void resume() {
    // 调用 coroutine_switch
    // 在汇编中保存 callee-saved 寄存器，设置协程函数。栈帧，然后将 rip 恢复到协程 yield 之后所需要执行的指令地址
    caller_registers[(int)Registers::RIP] = (uint64_t)context_ret;
    coroutine_switch(caller_registers,callee_registers);
  }
  // [*]目前并没有关于这个函数的引用，因此应该是需要自己增加对它的ref
  // [*]但应该并不是直接在yield中使用，因为那里是暂停运行，这里是恢复运行

  virtual void run() { CALL(f, args); }
  // [*]run函数的本质就是调用
};