#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "friendmodel.hpp"
#include "json.hpp"
#include "redis.hpp"
#include "offlinemessagemodel.hpp"
#include "usermodel.hpp"
#include "groupmodel.hpp"
#include <functional>
#include <muduo/net/TcpConnection.h>
#include <mutex>
#include <unordered_map>

using namespace std;
using namespace muduo;
using namespace muduo::net;

using json = nlohmann::json;

using MsgHandler =
    std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

// 聊天服务器业务类
class ChatService {
public:
  // 获取单例对象的接口
  static ChatService *instance(); // 处理登录业务
  void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 处理注册业务
  void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void clientCloseExcetion(const TcpConnectionPtr &conn);

  void reset();

  void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);

  void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);

  // 从redis消息队列中获取订阅的消息
  void handleRedisSubscribeMessage(int, string);
  MsgHandler getHandler(int msgid);

private:
  ChatService();
  // 存储消息id和其对应的业务处理方法
  unordered_map<int, MsgHandler> msgHandlerMap_;

  // 存储在线用户的通信连接
  unordered_map<int, TcpConnectionPtr> userConnMap_;

  // 互斥锁
  mutex connMutex_;

  // 用户数据操作对象
  UserModel userModel_;

  OfflineMsgModel offlineMessageModel_;

  FriendModel friendModel_;

  GroupModel groupModel_;

  Redis redis_;
};

#endif