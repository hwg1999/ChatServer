#include "EventLoopThread.hpp"
#include "EventLoop.hpp"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_(nullptr), exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name), mutex_(),
      cond_(), callback_(cb) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != nullptr) {
    loop_->quit();
    thread_.join();
  }
}

EventLoop *EventLoopThread::startLoop() {
  // 启动底层的新线程
  thread_.start();
  EventLoop *loop = nullptr;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    // 等待loop初始化完毕
    while (loop_ == nullptr) {
      cond_.wait(lock); // 保证线程函数准备工作完成，EventLoop指针对象不为空
    }
    loop = loop_;
  }
  return loop;
}

// 该函数是EventLoopThread类的核心函数，作用是启动loop循环
// 该函数和上面的startLoop函数并发执行，所以需要上锁和condition
void EventLoopThread::threadFunc() {
  // 创建一个独立的eventloop，one loop per thread
  EventLoop loop;
  if (callback_) {
    callback_(&loop); // 执行初始化回调函数
  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    // loop_指针指向了这个创建的栈上的对象，threadFunc退出之后，这个指针就失效了
    loop_ = &loop;
    cond_.notify_one();
  }

  loop.loop(); // 线程中执行事件循环
  std::unique_lock<std::mutex> lock(mutex_);
  loop_ = nullptr;
}