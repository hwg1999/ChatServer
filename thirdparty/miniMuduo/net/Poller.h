#pragma once

#include "Timestamp.h"
#include "noncopyable.h"

#include <unordered_map>
#include <vector>

class Channel;
class EventLoop;

// 核心IO复用模块
class Poller : noncopyable {
public:
  using ChannelList = std::vector<Channel *>;

  Poller(EventLoop *loop);
  virtual ~Poller() = default;

  // 给所有IO复用保留统一的接口
  virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
  virtual void updateChannel(Channel *channel) = 0;
  virtual void removeChannel(Channel *channel) = 0;

  // 判断channel是否在当前Poller当中
  bool hasChannel(Channel *channel) const;

  // EventLoop可以通过该接口获取默认的IO复用的具体实现
  static Poller *newDefaultPoller(EventLoop *loop);

protected:
  // map的key：sockfd或者eventfd，value：sockfd所属的channel通道
  using ChannelMap = std::unordered_map<int, Channel *>;
  // 保存所有fd对应的通道
  ChannelMap channels_;

private:
  // Poller所属的事件循环
  EventLoop *ownerLoop_;
};