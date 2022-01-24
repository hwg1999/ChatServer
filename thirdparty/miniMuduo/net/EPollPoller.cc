#include <errno.h>
#include <strings.h>
#include <unistd.h>

#include "Channel.hpp"
#include "EPollPoller.hpp"
#include "Logger.hpp"

namespace {
// channel未添加到poller中
const int kNew = -1;
// channel已添加到poller中
const int kAdded = 1;
// channel从poller中删除
const int kDeleted = 2;
}; // namespace

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) // vector<epoll_event>
{
  if (epollfd_ < 0) {
    LOG_FATAL("epoll_create error:%d ", errno);
  }
}

EPollPoller::~EPollPoller() { ::close(epollfd_); }

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  LOG_DEBUG("func=%s => fd total count:%lu", __FUNCTION__, channels_.size());

  int numEvents = ::epoll_wait(epollfd_, events_.data(),
                               static_cast<int>(events_.size()), timeoutMs);

  if (numEvents > 0) {
    LOG_DEBUG("func=%s => %d events happened ", __FUNCTION__, numEvents);
    fillActiveChannels(numEvents, activeChannels); //把有IO事件的channel加入到activeChannels中,
                                                   //EventLoop统一处理
    if (numEvents == events_.size()) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    LOG_DEBUG("func=%s => timeout! ", __FUNCTION__);
  } else {
    if (errno != EINTR) {
      LOG_ERROR("EPollPoller::poll() err!"); // TODO
    }
  }

  return Timestamp::now();
}

void EPollPoller::updateChannel(Channel *channel) {
  const int index = channel->index();
  LOG_DEBUG("func=%s => fd=%d events=%d index=%d ", __FUNCTION__,
           channel->fd(), channel->events(), index);
  //没在或者曾经在epoll队列中，添加到epoll队列中
  if (index == kNew || index == kDeleted) {
    if (index == kNew) { //没在epoll队列中
      int fd = channel->fd();
      channels_[fd] = channel; // channels_中添加 fd->channel键值对
    } else {
      // index == kDeleted  // 曾经在epoll队列中
    }
    channel->set_index(kAdded);     //修改index为已在队列中kAdded（1）
    update(EPOLL_CTL_ADD, channel); // 注册事件
  } else { //如果就在epoll队列中的，若没有关注事件了就暂时删除，如果有关注事件，就修改
    int fd = channel->fd();
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel); // 删除事件
      channel->set_index(kDeleted); // channel标记kDeleted（2）暂时删除
    } else {
      update(EPOLL_CTL_MOD, channel); // 更新事件
    }
  }
}

// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel) {
  int fd = channel->fd();
  channels_.erase(fd);
  LOG_DEBUG("func=%s => fd=%d", __FUNCTION__, fd);
  //标志位必须是kAdded或者kDeleted
  int index = channel->index();
  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel); //从epoll队列中删除这个channel
  }
  channel->set_index(kNew); //设置标志位是kNew(-1) ，相当于完全删除
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList *activeChannels) const {
  for (int i = 0; i < numEvents; ++i) {
    // 获取当前就绪IO事件对应的channel
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr); 
    channel->set_revents(events_[i].events);
    // EventLoop通过activeChannels得到活跃连接
    activeChannels->push_back(channel);
  }
}

// 更新channel通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel) {
  epoll_event event;
  bzero(&event, sizeof event);

  int fd = channel->fd();

  event.events = channel->events();
  event.data.fd = fd;
  event.data.ptr = channel;
  // todo
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG_ERROR("epoll_ctl del error:%d\n", errno);
    } else {
      LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
    }
  }
}