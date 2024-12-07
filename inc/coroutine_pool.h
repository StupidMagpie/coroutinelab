#pragma once
#include "context.h"
#include <memory>
#include <thread>
#include <vector>
#include <cstdio>
struct coroutine_pool;
extern coroutine_pool *g_pool;

/**
 * @brief 协程池
 * 保存所有需要同步执行的协程函数。并可以进行并行/串行执行。
 */
struct coroutine_pool {
  std::vector<basic_context *> coroutines;
  int context_id;

  // whether run in threads or coroutines
  bool is_parallel;

  ~coroutine_pool() {
    for (auto context : coroutines) {
      delete context;
    }
  }

  // add coroutine to pool
  template <typename F, typename... Args>
  void new_coroutine(F f, Args... args) {       // 第一个参数：协程要执行的函数；后面的变长参数代表这个函数的参数
    coroutines.push_back(new coroutine_context(f, args...));
  }

  /**
   * @brief 以并行多线程的方式执行所有协程函数
   */
  void parallel_execute_all() {
    g_pool = this;
    is_parallel = true;
    std::vector<std::thread> threads;
    for (auto p : coroutines) {     // 这里每一个p都是basic_context类
      threads.emplace_back([p]() { p->run(); });
    }

    for (auto &thread : threads) {
      thread.join();
    }
  }

  /**
   * @brief 以协程执行的方式串行并同时执行所有协程函数
   * TODO: Task 1, Task 2
   * 在 Task 1 中，我们不需要考虑协程的 ready
   * 属性，即可以采用轮询的方式挑选一个未完成执行的协程函数进行继续执行的操作。
   * 在 Task 2 中，我们需要考虑 sleep 带来的 ready
   * 属性，需要对协程函数进行过滤，选择 ready 的协程函数进行执行。
   *
   * 当所有协程函数都执行完毕后，退出该函数。
   */

  // [*]将协程池管理函数也当作一个特殊的“协程”，它也会中断（对应协程的yield())
  void serial_execute_all() {
    is_parallel = false;
    g_pool = this;
    int jobNum = coroutines.size();
    
    while(jobNum>0){
      int count=0;
      jobNum=0;
      for (auto context : coroutines) {
        // [*]如何通过一个context执行其协程 -> 查找context.h

        // [*]是否需要添加一个条件？
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
        count++;
      }
    }

    
    /*
    int jobSize;
    int index;
    int count;
    while(jobSize){
      count=0;
      index=0;
      jobSize = coroutines.size();
      while(index < jobSize){
        auto context = coroutines[index];
        if(!context->finished){
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
          index++;
          count++;
        }
        else{
          //已经完成就将其删除
          coroutines.erase(coroutines.begin()+index);
          jobSize--;
          //此时index不用增加，count也不用增加
        }
      }
    }
    */
    coroutines.clear();
  }
};
