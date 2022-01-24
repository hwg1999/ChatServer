#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Acceptor.hpp"
#include "Buffer.hpp"
#include "Callbacks.hpp"
#include "EventLoop.hpp"
#include "EventLoopThreadPool.hpp"
#include "InetAddress.hpp"
#include "TcpConnection.hpp"
#include "noncopyable.hpp"

// 对外的服务器编程使用的类
class TcpServer : noncopyable {
public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  enum Option {
    kNoReusePort,
    kReusePort,
  };
  //传入TcpServer所属的loop，本地ip，服务器名
  TcpServer(EventLoop *loop, const InetAddress &listenAddr,
            const std::string &nameArg, Option option = kNoReusePort);
  ~TcpServer();
  // 连接建立、断开，消息到达，消息完全写入tcp内核发送缓冲区 的回调函数
  // 非线程安全，但是都在IO线程中调用
  void setThreadInitcallback(const ThreadInitCallback &cb) {
    threadInitCallback_ = cb;
  }
  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }
  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }

  /// 设定线程池中线程的数量（sub reactor的个数），需在start()前调用
  // 0： 单线程(accept 和 IO 在一个线程)
  // 1： acceptor在一个线程，所有IO在另一个线程
  // N： acceptor在一个线程，所有IO通过round-robin分配到N个线程中
  void setThreadNum(int numThreads);

  //启动线程池管理器，将Acceptor::listen()加入调度队列（只启动一次）
  void start();

private:
  using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
  // todo why not thread safe
  //传给Acceptor，Acceptor会在有新的连接到来时调用->handleRead()
  void newConnection(int sockfd, const InetAddress &peerAddr);
  //移除连接
  void removeConnection(const TcpConnectionPtr &conn); 
  // 将连接从loop中移除
  void removeConnectionInLoop(const TcpConnectionPtr &conn);

  EventLoop *loop_; // baseLoop 用户定义的loop
  
  const std::string ipPort_;
  const std::string name_;
  // 仅由TcpServer持有
  std::unique_ptr<Acceptor> acceptor_; // 运行在mainLoop，任务就是监听新连接事件
  std::shared_ptr<EventLoopThreadPool> threadPool_;

  ConnectionCallback connectionCallback_;       // 有新连接时的回调
  MessageCallback messageCallback_;             // 有读写消息时的回调
  WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调

  ThreadInitCallback threadInitCallback_; // loop线程初始化的回调

  std::atomic_int started_;
  
  int nextConnId_; // 下一个连接ID，用于生成TcpConnection的name
  ConnectionMap connections_; // 保存所有的连接
};