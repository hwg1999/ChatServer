#pragma once

#include "Thread.h"
#include "noncopyable.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

class EventLoop;

class EventLoopThread : noncopyable {
public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                  const std::string &name = std::string());
  ~EventLoopThread();
  //启动成员thread_线程，该线程就成了I/O线程，内部调用thread_.start()
  EventLoop *startLoop();

private:
  void threadFunc(); //线程运行函数

  EventLoop *loop_;              // 指向当前线程创建的EventLoop对象
  bool exiting_;                 // 退出线程标志位
  Thread thread_;                // 线程
  std::mutex mutex_;             // 互斥锁，保护loop
  std::condition_variable cond_; // 用于保证loop初始化成功
  ThreadInitCallback callback_;  // 线程初始化的回调函数
};