#pragma once

#include <vector>

#include <sys/epoll.h>

#include "Poller.h"
#include "Timestamp.h"

class Channel;

/**
 *  对epoll的封装
 */
class EPollPoller : public Poller {
public:
  EPollPoller(EventLoop *loop);
  ~EPollPoller() override;

  // 重写基类Poller的抽象方法
  Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
  void updateChannel(Channel *channel) override;
  void removeChannel(Channel *channel) override;

private:
  using EventList = std::vector<epoll_event>;
  // 填写活跃的连接
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
  // 更新channel
  void update(int operation, Channel *channel);

  int epollfd_;      // epoll占用的fd
  EventList events_; // 当前监听的 epoll_event 数组

  static const int kInitEventListSize = 16; // 初始的事件列表大小为16
};