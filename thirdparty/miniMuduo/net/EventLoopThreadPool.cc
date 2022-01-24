#include "EventLoopThreadPool.hpp"
#include "EventLoopThread.hpp"

#include <memory>

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop,
                                         const std::string &nameArg)
    : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0),
      next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {}

void EventLoopThreadPool::start(const ThreadInitCallback &cb) {
  started_ = true;

  for (int i = 0; i < numThreads_; ++i) {
    char buf[name_.size() + 32];
    snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
    // 启动EventLoopThread线程，在进入事件循环之前，会调用cb
    EventLoopThread *t = new EventLoopThread(cb, buf);
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));
    // 底层创建线程，绑定一个新的EventLoop，并返回该loop的地址
    loops_.push_back(t->startLoop());
  }

  // 整个服务端只有一个线程，只运行baseloop
  if (numThreads_ == 0 && cb) {
    cb(baseLoop_);
  }
}

// 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
EventLoop *EventLoopThreadPool::getNextLoop() {
  EventLoop *loop = baseLoop_; // 假设loops_为空，则loop指向baseLoop_

  // 假设不为空，依照round-robin的调度方式选择一个EventLoop
  if (!loops_.empty()) {
    loop = loops_[next_];
    ++next_;
    if (next_ >= loops_.size()) {
      next_ = 0;
    }
  }

  return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops() {
  if (loops_.empty()) {
    return std::vector<EventLoop *>(1, baseLoop_);
  } else {
    return loops_;
  }
}