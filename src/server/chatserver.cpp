#include "chatserver.h"
#include "json.hpp"
#include "chatservice.h"

#include <functional>

using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop, const InetAddress &listenAddr,
                       const string &nameArg)
    : server_(loop, listenAddr, nameArg), loop_(loop) {
  // 注册连接回调
  server_.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));
  // 注册消息回调
  server_.setMessageCallback(
      std::bind(&ChatServer::onMessage, this, _1, _2, _3));
  // 设置线程数量
  server_.setThreadNum(4);
}

void ChatServer::start() { server_.start(); }

// 上报连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn) {
  // 客户端断开连接
  if (!conn->connected()) {
    ChatService::instance()->clientCloseExcetion(conn);
    conn->shutdown();
  }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp time) {
  string buf = buffer->retrieveAllAsString();
  json js = json::parse(buf);
  auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
  // 回调消息绑定好的事件处理器，来执行相应的业务处理
  msgHandler(conn, js, time);
}