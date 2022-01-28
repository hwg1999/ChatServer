#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <sys/eventfd.h>
#include <unistd.h>

#include "Channel.hpp"
#include "CurrentThread.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"
#include "Poller.hpp"

// 防止一个线程创建多个EventLoop   thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 1000000;

// 创建wakeupfd，用来唤醒subReactor处理新来的channel
int createEventfd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG_FATAL("eventfd error:%d \n", errno);
  }
  return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),                 // 是否已经开始了IO事件循环
      quit_(false),                    // 是否退出
      callingPendingFunctors_(false),  // 是否正在处理IO事件
      threadId_(CurrentThread::tid()), // 当前线程的tid
      poller_(Poller::newDefaultPoller(
          this)), // Poller对象(监听socketfd封装的Channels)
      wakeupFd_(createEventfd()), // eventfd描述符，用于唤醒阻塞的poll
      wakeupChannel_(new Channel(this, wakeupFd_)) // eventfd封装的Channel
{
  LOG_DEBUG("func=%s => EventLoop %p created  in thread %d ", __FUNCTION__, this,
            threadId_);
  if (t_loopInThisThread) {
    LOG_FATAL("Another EventLoop %p exists in this thread %d ",
              t_loopInThisThread, threadId_);
  } else {
    t_loopInThisThread = this;
  }
  // TODO where the timestamp
  // 设置唤醒channel的可读事件回调，并注册到Poller中, 将消息读走防止反复唤醒
  wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop() {
  looping_ = true;
  quit_ = false;

  LOG_DEBUG("func=%s => EventLoop %p start looping ", __FUNCTION__, this);

  while (!quit_) {
    activeChannels_.clear();
    // 监听两类fd   一种是已连接fd，一种是wakeupfd
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
    for (Channel *channel : activeChannels_) {
      // Poller监听哪些channel发生事件了，然后上报给EventLoop，EventLoop通知channel处理相应的事件
      channel->handleEvent(pollReturnTime_);
    }
    // 执行当前EventLoop事件循环需要处理的回调操作
    doPendingFunctors();
  }

  LOG_DEBUG("func=%s => EventLoop %p stop looping. ", __FUNCTION__, this);
  looping_ = false;
}

/**
 *   退出事件循环
 *   1.loop在自己的线程中调用quit
 *   2.在非loop的线程中，调用loop的quit
 */
void EventLoop::quit() {
  quit_ = true;

  // 如果当前线程并不是该loop对应的线程，则唤醒其对应的loop线程
  if (!isInLoopThread()) {
    wakeup();
  }
}

// 在当前loop中执行cb  todo where the runinloop called
void EventLoop::runInLoop(Functor cb) {
  if (isInLoopThread()) {
    cb(); // 如果是在IO线程调用，则直接执行
  } else { // 其他线程调用，就先放入到任务队列，之后在loop中执行
    LOG_DEBUG(
        "queueInloop, current threadId = %d, this cb should run in thread : %d",
        CurrentThread::tid(), threadId_);
    queueInLoop(cb);
  }
}

// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb) {
  { // 加锁，放入待执行的任务队列中；大括号减小临界区长度，降低锁竞争
    std::unique_lock<std::mutex> lock(mutex_);
    pendingFunctors_.emplace_back(cb);
  }
  // 根据情况选择是否唤醒
  // callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调
  if (!isInLoopThread() || callingPendingFunctors_) {
    wakeup(); // 唤醒loop所在线程
  }
}

// 读取wakeupfd的数据，以防反复唤醒
void EventLoop::handleRead() {
  uint64_t one = 1;
  ssize_t n = read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
  }
}

// 通过往eventfd写标志通知，让阻塞poll立马返回并执行回调函数
void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
  }
}

// 将channel的事件和回调注册到Poller中
void EventLoop::updateChannel(Channel *channel) {
  poller_->updateChannel(channel);
}

// 将channel的事件和回调从Poller中移除
void EventLoop::removeChannel(Channel *channel) {
  poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel) {
  return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors_ = true; // 执行计算任务中

  // 加锁，把回调任务队列swap到临时变量中；降低临界区长度
  {
    std::unique_lock<std::mutex> lock(mutex_);
    functors.swap(pendingFunctors_);
  }
  // 依次执行队列中的用户回调任务
  for (const Functor &functor : functors) {
    functor();
  }

  callingPendingFunctors_ = false;
}