#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include "friendmodel.h"
#include "groupmodel.h"
#include "json.hpp"
#include "offlinemessagemodel.h"
#include "redis.h"
#include "usermodel.h"

#include "TcpConnection.h"

#include <functional>
#include <mutex>
#include <unordered_map>

using namespace std;
using json = nlohmann::json;
using MsgHandler =
    std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp)>;

// 聊天服务器业务类
class ChatService {
public:
  // 获取单例对象的接口
  static ChatService *instance(); 
  // 处理登录业务
  void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 处理注册业务
  void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 私聊
  void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 处理关闭事件
  void clientCloseExcetion(const TcpConnectionPtr &conn);
  // 重置状态
  void reset();
  // 添加好友
  void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 创建群组
  void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 退出登录
  void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 加群
  void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
  // 群聊
  void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);

  // 从redis消息队列中获取订阅的消息
  void handleRedisSubscribeMessage(int, string);
  // 获取msgid对应的handler
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