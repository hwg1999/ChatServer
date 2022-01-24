#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

#include "noncopyable.hpp"

class Thread : noncopyable {
public:
  using ThreadFunc = std::function<void()>;

  explicit Thread(ThreadFunc, const std::string &name = std::string());
  ~Thread();

  void start();
  void join();

  bool started() const { return started_; }
  pid_t tid() const { return tid_; }
  const std::string &name() const { return name_; }

  static int numCreated() { return numCreated_; }

private:
  void setDefaultName(); // 默认线程名称 Thread_num

  bool started_;                        // 线程是否开始运行
  bool joined_;                         // 线程是否可join
  std::shared_ptr<std::thread> thread_; // 线程变量
  pid_t tid_;                           // 线程tid
  ThreadFunc func_;                     // 线程回调函数
  std::string name_;                    // 线程名称
  static std::atomic_int numCreated_; // 原子操作，当前已经创建线程的数量
};