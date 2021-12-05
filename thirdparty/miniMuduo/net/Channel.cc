#include "Channel.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"

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

// 当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件
void Channel::update() {
  // 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
  loop_->updateChannel(this);
}

// 在channel所属的EventLoop中， 把当前的channel删除掉
void Channel::remove() { loop_->removeChannel(this); }

/**
 * fd接到loop通知后处理时间，这里如果有tied_就要将tie_提升为share_ptr
 * 如果提升失败则不回调
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

// 根据poller通知的channel发生的具体事件， 由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime) {
  LOG_INFO("channel handleEvent revents:%d", revents_);

  // 客户端关闭连接
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    if (closeCallback_) {
      closeCallback_();
    }
  }

  // 出错
  if (revents_ & EPOLLERR) {
    if (errorCallback_) {
      errorCallback_();
    }
  }
  // 可读
  if (revents_ & (EPOLLIN | EPOLLPRI)) {
    if (readCallback_) {
      readCallback_(receiveTime);
    }
  }
  // 可写
  if (revents_ & EPOLLOUT) {
    if (writeCallback_) {
      writeCallback_();
    }
  }
}