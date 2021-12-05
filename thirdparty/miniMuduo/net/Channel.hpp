#pragma once

#include "Timestamp.hpp"
#include "noncopyable.hpp"

#include <functional>
#include <memory>

class EventLoop;

/**
 *    Channel是底层通过Socket建立TCP通信的通道，用于事件分发。
 * Channel对fd的事件相关方法进行了封装，例如负责注册fd的可读或
 * 可写事件到EvenLoop，又如fd产生事件后要如何响应。Channel与fd
 * 是聚合关系，一个fd对应一个channel，不实际拥有fd，Channel析构
 * 是不会close当前fd的。文件描述符fd可以是socketfd或eventfd。
 */
class Channel : noncopyable {
public:
  using EventCallback = std::function<void()>;
  using ReadEventCallback = std::function<void(Timestamp)>;

  Channel(EventLoop *loop, int fd);
  ~Channel();

  // fd得到poller通知以后，处理事件的
  void handleEvent(Timestamp receiveTime);

  // 设置回调函数对象
  void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  // 防止当channel还在执行回调操作时，channel被手动remove掉。
  void tie(const std::shared_ptr<void> &);

  // get & set
  int fd() const { return fd_; }
  int events() const { return events_; }
  void set_revents(int revt) { revents_ = revt; }
  int index() { return index_; }
  void set_index(int idx) { index_ = idx; }
  void enableReading() {
    events_ |= kReadEvent;
    update();
  }
  void disableReading() {
    events_ &= ~kReadEvent;
    update();
  }
  void enableWriting() {
    events_ |= kWriteEvent;
    update();
  }
  void disableWriting() {
    events_ &= ~kWriteEvent;
    update();
  }
  void disableAll() {
    events_ = kNoneEvent;
    update();
  }

  // 返回fd当前的事件状态
  bool isNoneEvent() const { return events_ == kNoneEvent; }
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }

  // one loop per thread
  EventLoop *ownerLoop() { return loop_; }
  void remove();

private:
  void update();
  void handleEventWithGuard(Timestamp receiveTime);

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop *loop_; // 事件循环
  const int fd_;    // fd, Poller监听的对象
  int events_;      // 注册fd感兴趣的事件
  int revents_;     // poller返回的具体发生的事件
  int index_; // 在poll中表示数组下标，在epoll中则表示三种状态

  // 用于延长Channel生存期
  std::weak_ptr<void> tie_;
  bool tied_;

  /* 因为channel通道能够从loop获知fd最终发生的
     具体的事件revents，所以它负责调用具体事件的回调操作 */
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};
