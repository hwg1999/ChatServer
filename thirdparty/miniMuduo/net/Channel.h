#pragma once

#include "Timestamp.h"
#include "noncopyable.h"

#include <functional>
#include <memory>

class EventLoop;

/**
 *    Channel是底层通过Socket建立TCP通信的通道，用于事件分发。
 * Channel对fd的事件相关方法进行了封装，例如负责注册fd的可读或
 * 可写事件到EvenLoop，又如fd产生事件后要如何响应。Channel与fd
 * 是聚合关系，一个fd对应一个channel，不实际拥有fd，Channel析构
 * 是不会close当前fd的。文件描述符fd可以是socketfd或eventfd。
 * sockfd用于读写，eventfd用于将当前loop唤醒。
 */
class Channel : noncopyable {
public:
  using EventCallback = std::function<void()>; // 事件回调函数定义
  using ReadEventCallback =
      std::function<void(Timestamp)>; // 读事件回调函数定义

  Channel(EventLoop *loop, int fd);
  ~Channel();

  // fd得到poller通知以后，处理事件
  void handleEvent(Timestamp receiveTime);

  // 设置回调函数对象
  void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  // 防止当channel还在执行回调操作时，被remove掉
  void tie(const std::shared_ptr<void> &);

  // get & set
  int fd() const { return fd_; }
  int events() const { return events_; } //返回注册的事件
  void set_revents(int revt) { revents_ = revt; } // 设置发生的事件:poller中调用
  // for poller
  int index() { return index_; }
  void set_index(int idx) { index_ = idx; }
  // 注册可读事件
  void enableReading() {
    events_ |= kReadEvent;
    update();
  }
  // 注销可读事件
  void disableReading() {
    events_ &= ~kReadEvent;
    update();
  }
  // 注册可写事件
  void enableWriting() {
    events_ |= kWriteEvent;
    update();
  }
  // 注销可写事件
  void disableWriting() {
    events_ &= ~kWriteEvent;
    update();
  }
  // 注销所有事件
  void disableAll() {
    events_ = kNoneEvent;
    update();
  }

  // 返回fd当前的事件状态
  // 判断是否注册了事件
  bool isNoneEvent() const { return events_ == kNoneEvent; }
  bool isWriting() const { return events_ & kWriteEvent; } // 是否注册可写事件
  bool isReading() const { return events_ & kReadEvent; } // 是否注册可读事件

  // 返回当前Channel所属loop
  EventLoop *ownerLoop() { return loop_; }
  void remove();

private:
  void update(); // 注册事件后更新到EventLoop
  void handleEventWithGuard(Timestamp receiveTime); // 加锁的事件处理

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop *loop_; // channel所属的EventLoop
  const int fd_;    // channel负责的文件描述符，但不负责关闭该文件描述符
  int events_;      // 注册的事件
  int revents_;     // poller返回接收到的就绪的事件
  int index_;       // 在poll中表示数组下标，在epoll中则表示三种状态

  // 用于延长Channel生存期
  std::weak_ptr<void> tie_;
  bool tied_;

  /* 因为channel通道能够从loop获知fd最终发生的
     具体的事件revents，所以它负责调用具体事件的回调操作 */
  ReadEventCallback readCallback_; //读事件回调
  EventCallback writeCallback_;    //写事件回调
  EventCallback closeCallback_;    //关闭事件回调
  EventCallback errorCallback_;    // 出错事件回调
};
