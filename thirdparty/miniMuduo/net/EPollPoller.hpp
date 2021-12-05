#pragma once

#include "Poller.hpp"
#include "Timestamp.hpp"

#include <sys/epoll.h>
#include <vector>

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
  // 填写活跃的连接
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
  // 更新channel
  void update(int operation, Channel *channel);

  using EventList = std::vector<epoll_event>;

  static const int kInitEventListSize = 16;

  int epollfd_;
  EventList events_;
};