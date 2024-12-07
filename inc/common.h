#pragma once
#include "context.h"
#include "coroutine_pool.h"
#include <cstdlib>

// 获取当前时间    __builtin_prefetch(&table[probe]);

auto get_time() { return std::chrono::system_clock::now(); }

/**
 * @brief yield函数
 *
 * TODO: Task 1
 * 协程主动暂停执行，保存协程的寄存器和栈帧。
 * 将上下文转换至 coroutine_pool.serial_execute_all() 中的上下文进行重新的
 * schedule 调用。
 * [*] 应该是在serial_execute_all中调用了协程，保存了当时的上下文；这里再将其取出来
 */
void yield() {
  if (!g_pool->is_parallel) {
    // 从 g_pool 中获取当前协程状态 [*]在序列化执行模式中，执行完一个后取出下一个
    auto context = g_pool->coroutines[g_pool->context_id];
    // [*]注意只是暂停，并不是结束！以后还会通过resume来恢复执行
    // [*]这里的context是正在运行的协程上下文
    context->callee_registers[(int)Registers::RIP] = (uint64_t)context_ret;
    coroutine_switch(context->callee_registers,context->caller_registers);
    // 调用 coroutine_switch 切换到 coroutine_pool 上下文
  }
}

/**
 * @brief 完成 sleep 函数
 *
 * TODO: Task 2
 * 你需要完成 sleep 函数。
 * 此函数的作用为：
 *  1. 将协程置为不可用状态。
 *  2. yield 协程。
 *  3. 在至少 @param ms 毫秒之后将协程置为可用状态。
 */

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
