#pragma once

#include <functional>

#include "Channel.h"
#include "Socket.h"
#include "noncopyable.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
public:
  using NewConnectionCallback =
      std::function<void(int sockfd, const InetAddress &)>;

  Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
  ~Acceptor();
  // 设置新连接建立的回调函数
  void setNewConnectionCallback(const NewConnectionCallback &cb) {
    newConnectionCallback_ = cb;
  }
  // 返回当前监听状态
  bool listenning() const { return listenning_; }
  void listen(); // 开启监听

private:
  void handleRead();

  EventLoop *loop_; // Acceptor用的就是用户定义的那个baseLoop，也称作mainLoop
  Socket acceptSocket_;   // 用于接收新连接的socket封装
  Channel acceptChannel_; // 封装acceptSocket_的channel，监听其上的事件
  NewConnectionCallback newConnectionCallback_; // 建立新连接时调用的回调函数
  bool listenning_;                             // 监听状态
};