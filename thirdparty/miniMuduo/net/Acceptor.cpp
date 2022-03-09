#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Acceptor.h"
#include "InetAddress.h"
#include "Logger.h"

static int createNonblocking() {
  int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (sockfd < 0) {
    LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__,
              __LINE__, errno);
  }
  return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr,
                   bool reuseport)
    : loop_(loop), acceptSocket_(createNonblocking()), // socket
      acceptChannel_(loop, acceptSocket_.fd()), listenning_(false) {
  // 设置服务端socket选项，并绑定到指定ip和port
  acceptSocket_.setReuseAddr(true);      // addr重用
  acceptSocket_.setReusePort(true);      // 端口重用
  acceptSocket_.bindAddress(listenAddr); // bind
  // 监听listenfd上的可读事件（新的连接）
  acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
  // 不关注socket上的IO事件，从EventLoop的Poller注销
  acceptChannel_.disableAll();
  acceptChannel_.remove();
}

void Acceptor::listen() {
  listenning_ = true;
  acceptSocket_.listen(); // listen
  acceptChannel_.enableReading();
}

// listenfd有事件发生了，就是有新用户连接了
void Acceptor::handleRead() {
  InetAddress peerAddr;
  int connfd = acceptSocket_.accept(&peerAddr); // 这里是真正接收连接
  if (connfd >= 0) {                            //新的连接成功
    if (newConnectionCallback_) {
      // 建立新的连接，调用TcpServer的回调，返回已连接的socketfd和peer端地址
      newConnectionCallback_(
          connfd,
          peerAddr); // 轮询找到subLoop，唤醒，分发当前的新客户端的Channel
    } else {
      // 若上层应用TcpServer未注册新连接回调函数，则直接关闭当前连接
      ::close(connfd);
    }
  } else { // 连接异常，处理服务端fd耗尽
    LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__,
              errno);
    if (errno == EMFILE) { // 无可用fd，如不处理，Poller水平触发模式下会一直触发
      LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__,
                __LINE__);
    }
  }
}