#pragma once
#include "noncopyable.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable {
public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
  ~EventLoopThreadPool();

  void setThreadNum(int numThreads) { numThreads_ = numThreads; }

  void start(const ThreadInitCallback &cb = ThreadInitCallback());

  // 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
  EventLoop *getNextLoop();

  std::vector<EventLoop *> getAllLoops();

  bool started() const { return started_; }
  const std::string name() const { return name_; }

private:
  EventLoop *baseLoop_; // Acceptor所属EventLoop
  std::string name_;    // 线程池名称
  bool started_;        // 是否已经启动
  int numThreads_;      // 线程数
  int next_;            // 新连接到来，所选择的EventLoop对象下标
  std::vector<std::unique_ptr<EventLoopThread>> threads_; // IO线程列表
  std::vector<EventLoop *> loops_;                        // EventLoop列表
};