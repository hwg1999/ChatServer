#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

// EventLoop: ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {}

Channel::~Channel() {}

// 该方法在TcpConnection成功建立连接的时候调用
void Channel::tie(const std::shared_ptr<void> &obj) {
  tie_ = obj;
  tied_ = true;
}

// 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
void Channel::update() { loop_->updateChannel(this); }

// 在channel所属的EventLoop中， 把当前的channel从监听队列中移除
void Channel::remove() { loop_->removeChannel(this); }

/**
 * fd接到loop通知后处理时间，这里如果有tied_就要获得对应的shared_ptr
 * 获取失败则说明对应连接已关闭
 */
void Channel::handleEvent(Timestamp receiveTime) {
  if (tied_) {
    std::shared_ptr<void> guard = tie_.lock();
    if (guard) {
      handleEventWithGuard(receiveTime);
    }
  } else {
    handleEventWithGuard(receiveTime);
  }
}

// 查看epoll/或者poll返回的具体是什么事件，并根据事件的类型进行相应的处理
void Channel::handleEventWithGuard(Timestamp receiveTime) {
  LOG_DEBUG("%s:channel handleEvent revents:%d", __FUNCTION__, revents_);

  // 客户端关闭连接
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    //当事件为挂起并没有可读事件时
    if (closeCallback_) {
      closeCallback_();
    }
  }

  //发生错误
  if (revents_ & EPOLLERR) {
    if (errorCallback_) {
      errorCallback_();
    }
  }
  //关于读的事件
  if (revents_ & (EPOLLIN | EPOLLPRI)) {
    if (readCallback_) {
      readCallback_(receiveTime);
    }
  }
  //关于写的事件
  if (revents_ & EPOLLOUT) {
    if (writeCallback_) {
      writeCallback_();
    }
  }
}