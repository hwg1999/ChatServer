#include "TcpConnection.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"
#include "Socket.hpp"

#include <errno.h>
#include <functional>
#include <netinet/tcp.h>
#include <string>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
  if (loop == nullptr) {
    LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__,
              __LINE__);
  }
  return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg,
                             int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)), name_(nameArg), state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)), // 已连接socketfd封装的Socket对象
      channel_(new Channel(loop, sockfd)), localAddr_(localAddr),
      peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024) // 缓冲区数据最大64M
{
  // 下面给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会回调相应的操作函数
  channel_->setReadCallback(
      std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
  channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
  channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
  channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
  LOG_DEBUG("func=%s => TcpConnection::ctor[%s] at fd=%d", __FUNCTION__, name_.c_str(), sockfd);
  // 设置保活机制
  socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
  LOG_DEBUG("func=%s => TcpConnection::dtor[%s] at fd=%d state=%d ", __FUNCTION__, name_.c_str(),
           channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &buf) {
  if (state_ == kConnected) {
    if (loop_->isInLoopThread()) {
      sendInLoop(buf.c_str(), buf.size());
    } else {
      loop_->runInLoop(
          std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
    }
  }
}

/**
 * 发送数据  应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，
 * 而且设置了水位回调
 */
void TcpConnection::sendInLoop(const void *data, size_t len) {
  // todo why not in loop thread
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;

  // 之前调用过该connection的shutdown，不能再进行发送了
  if (state_ == kDisconnected) {
    LOG_ERROR("disconnecte, gdive up writing!");
    return;
  }
  // 如果当前channel没有写事件发生，并且发送buffer无待发送数据，那么直接发送
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
    nwrote = ::write(channel_->fd(), data, len);
    if (nwrote >= 0) { // 发送正常
      remaining = len - nwrote;
      // 如果全部发送完毕，就调用发送完成回调函数
      if (remaining == 0 && writeCompleteCallback_) {
        // 既然在这里数据全部发送完成，就不用给channel设置EPOLLOUT事件了
        loop_->queueInLoop(
            std::bind(writeCompleteCallback_, shared_from_this()));
      }
    } else { // nwrote < 0   一旦发生错误，handleRead()读到0字节，关闭连接
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG_ERROR("TcpConnection::sendInLoop");
        if (errno == EPIPE || errno == ECONNRESET) 
        {
          faultError = true; // 仅记录错误原因
        }
      }
    }
  }
  // 没有经过直接发送remaining == len，或者经过直接发送但还有剩余数据remaining < len
  // 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，然后给channel
  // 注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的channel，调用writeCallback_回调方法
  // 也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成
  if (!faultError && remaining > 0) {
    // 目前发送缓冲区剩余的待发送数据的长度
    size_t oldLen = outputBuffer_.readableBytes();
    // 如果输出缓冲区的数据已经超过高水位标记，那么调用highWaterMarkCallback_
    if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ &&
        highWaterMarkCallback_) {
      loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(),
                                   oldLen + remaining));
    }
    // 把数据添加到输出缓冲区中
    outputBuffer_.append((char *)data + nwrote, remaining);
    // 监听channel的可写事件（因为还有数据未发完），
    // 当可写事件被触发，就可以继续发送了，调用的是TcpConnection::handleWrite()
    if (!channel_->isWriting()) {
      // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout
      channel_->enableWriting();
    }
  }
}

// 关闭连接
void TcpConnection::shutdown() {
  if (state_ == kConnected) {
    setState(kDisconnecting);
    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
  }
}

void TcpConnection::shutdownInLoop() {
  if (!channel_->isWriting()) // 说明outputBuffer中的数据已经全部发送完成
  {
    socket_->shutdownWrite(); // 关闭写端
  }
}

// 连接建立
void TcpConnection::connectEstablished() {
  setState(kConnected);
  channel_->tie(shared_from_this());
  // 每个连接对应一个channel，打开描述符的可读属性
  channel_->enableReading();
  // 连接成功，回调用户注册的函数（比如OnConnection）
  connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed() {
  if (state_ == kConnected) {
    // 这里的代码块和 handleClose() 中重复
    setState(kDisconnected);
    channel_->disableAll(); // 把channel的所有感兴趣的事件，从poller中del掉
    connectionCallback_(shared_from_this());
  }
  channel_->remove(); // 把channel从poller中删除掉
}

void TcpConnection::handleRead(Timestamp receiveTime) {
  // TODO errno 是线程独占一份的吗，
  int savedErrno = 0;
  // 读数据到inputBuffer_中
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0) {
    // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  } else if (n == 0) {
    handleClose(); // 读到了0，表明客户端已经关闭了
  } else {
    errno = savedErrno;
    LOG_ERROR("TcpConnection::handleRead");
    handleError();
  }
}

void TcpConnection::handleWrite() {
  if (channel_->isWriting()) { // 当前channel的socketfd可写（可写事件）
    int savedErrno = 0;
    ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
    if (n > 0) {
      outputBuffer_.retrieve(n);
      if (outputBuffer_.readableBytes() == 0) { // 发送完毕
        channel_->disableWriting(); // 不再关注fd的可写事件，避免busy loop
        if (writeCompleteCallback_) {
          /// 通知用户，发送完毕。（但不保证对端是否完整接收处理）
          loop_->queueInLoop(
              std::bind(writeCompleteCallback_, shared_from_this()));
        }
        // 如果当前状态是正在关闭连接，主动发起关闭
        if (state_ == kDisconnecting) {
          shutdownInLoop();
        }
      }
    } else {
      LOG_ERROR("TcpConnection::handleWrite");
    }
  } else { // 已经不可写，不再发送
    LOG_ERROR("TcpConnection fd=%d is down, no more writing \n",
              channel_->fd());
  }
}

// 当对端调用shutdown()关闭连接时，本端会收到一个FIN，
// channel的读事件被触发，但inputBuffer_.readFd() 会返回0，然后调用
// handleClose()，处理关闭事件，最后调用TcpServer::removeConnection()。
// poller => channel::closeCallback => TcpConnection::handleClose
void TcpConnection::handleClose() {
  LOG_DEBUG("func=%s => TcpConnection::handleClose fd=%d state=%d ", __FUNCTION__, channel_->fd(),
           (int)state_);
  setState(kDisconnected); // 设置为已断开状态
  channel_->disableAll();  // channel上不再关注任何事情

  TcpConnectionPtr connPtr(shared_from_this()); // 必须使用智能指针
  connectionCallback_(connPtr); // 回调用户的连接处理回调函数
  // 必须最后调用，回调TcpServer的函数，TcpConnection的生命期由TcpServer控制
  // 关闭连接的回调  执行的是TcpServer::removeConnection回调方法
  closeCallback_(connPtr);
}

void TcpConnection::handleError() {
  int optval;
  socklen_t optlen = sizeof optval;
  int err = 0;
  if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) <
      0) {
    err = errno;
  } else {
    err = optval;
  }
  LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n",
            name_.c_str(), err);
}