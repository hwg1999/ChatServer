#include "chatservice.hpp"
#include "Logger.hpp"
#include "public.hpp"

#include <vector>

ChatService *ChatService::instance() {
  static ChatService service;
  return &service;
}

ChatService::ChatService() {
  msgHandlerMap_.insert(
      {LOGIN_MSG, std::bind(&ChatService::login, this, std::placeholders::_1,
                            std::placeholders::_2, std::placeholders::_3)});
  msgHandlerMap_.insert(
      {REG_MSG, std::bind(&ChatService::reg, this, std::placeholders::_1,
                          std::placeholders::_2, std::placeholders::_3)});
  msgHandlerMap_.insert(
      {ONE_CHAT_MSG,
       std::bind(&ChatService::oneChat, this, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3)});
  msgHandlerMap_.insert(
      {ADD_FRIEND_MSG,
       std::bind(&ChatService::addFriend, this, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3)});

  msgHandlerMap_.insert(
      {CREATE_GROUP_MSG,
       std::bind(&ChatService::createGroup, this, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3)});
  msgHandlerMap_.insert(
      {ADD_GROUP_MSG,
       std::bind(&ChatService::addGroup, this, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3)});
  msgHandlerMap_.insert(
      {GROUP_CHAT_MSG,
       std::bind(&ChatService::groupChat, this, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3)});
  msgHandlerMap_.insert(
      {LOGINOUT_MSG,
       std::bind(&ChatService::loginout, this, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3)});

  if (redis_.connect()) {
    redis_.init_notify_handler(
        std::bind(&ChatService::handleRedisSubscribeMessage, this,
                  std::placeholders::_1, std::placeholders::_2));
  }
}

void ChatService::reset() { userModel_.resetState(); }

MsgHandler ChatService::getHandler(int msgid) {
  auto it = msgHandlerMap_.find(msgid);
  if (it == msgHandlerMap_.end()) {
    return [=](const TcpConnectionPtr &conn, json &js, Timestamp time) {
      LOG_ERROR("msgid: %d can not find handler", msgid);
    };
  } else {
    return msgHandlerMap_[msgid];
  }
}

void ChatService::login(const TcpConnectionPtr &conn, json &js,
                        Timestamp time) {
  int id = js["id"].get<int>();
  string pwd = js["password"];

  User user = userModel_.query(id);
  if (user.getId() == id && user.getPwd() == pwd) {
    if (user.getState() == "online") {
      json response;
      response["msgid"] = LOGIN_MSG_ACK;
      response["errno"] = 2;
      response["errmsg"] = "this account is using!";
      conn->send(response.dump());
    } else {
      {
        lock_guard<mutex> lock(connMutex_);
        userConnMap_.insert({id, conn}); // 这里要考虑线程安全问题
      }
      redis_.subscribe(id);
      user.setState("online");
      userModel_.updateState(user);
      json response;
      response["msgid"] = LOGIN_MSG_ACK;
      response["errno"] = 0;
      response["id"] = user.getId();
      response["name"] = user.getName();
      // 查询该用户是否有离线消息
      vector<string> vec = offlineMessageModel_.query(id);
      if (!vec.empty()) {
        response["offlinemsg"] = vec;
        offlineMessageModel_.remove(id);
      }

      vector<User> userVec = friendModel_.query(id);
      if (!userVec.empty()) {
        vector<string> vec2;
        for (User &user : userVec) {
          json js;
          js["id"] = user.getId();
          js["name"] = user.getName();
          js["state"] = user.getState();
          vec2.push_back(js.dump());
        }
        response["friends"] = vec2;
      }

      vector<Group> groupuserVec = groupModel_.queryGroups(id);
      if (!groupuserVec.empty()) {
        vector<string> groupV;
        for (Group &group : groupuserVec) {
          json grpjson;
          grpjson["id"] = group.getId();
          grpjson["groupname"] = group.getName();
          grpjson["groupdesc"] = group.getDesc();
          vector<string> userV;
          for (GroupUser &user : group.getUsers()) {
            json js;
            js["id"] = user.getId();
            js["name"] = user.getName();
            js["state"] = user.getState();
            js["role"] = user.getRole();
            userV.push_back(js.dump());
          }
          grpjson["users"] = userV;
          groupV.push_back(grpjson.dump());
        }
        response["groups"] = groupV;
      }
      conn->send(response.dump());
    }
  } else {
    json response;
    response["msgid"] = LOGIN_MSG_ACK;
    response["errno"] = 1;
    response["errmsg"] = "id or password invalid";
    conn->send(response.dump());
  }
}

void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time) {
  string name = js["name"];
  string pwd = js["password"];

  User user;
  user.setName(name);
  user.setPwd(pwd);
  bool state = userModel_.insert(user);
  if (state) {
    json response;
    response["msgid"] = REG_MSG_ACK;
    response["errno"] = 0;
    response["id"] = user.getId();
    conn->send(response.dump());
  } else {
    json response;
    response["msgid"] = REG_MSG_ACK;
    response["errno"] = 1;
    conn->send(response.dump());
  }
}

void ChatService::clientCloseExcetion(const TcpConnectionPtr &conn) {
  User user;
  {
    lock_guard<mutex> lock(connMutex_);
    for (auto it = userConnMap_.begin(); it != userConnMap_.end(); ++it) {
      if (it->second == conn) {
        user.setId(it->first);
        userConnMap_.erase(it);
        break;
      }
    }
  }

  redis_.unsubscribe(user.getId());

  if (user.getId() != -1) {
    user.setState("offline");
    userModel_.updateState(user);
  }
}

void ChatService::oneChat(const TcpConnectionPtr &conn, json &js,
                          Timestamp time) {
  int toid = js["toid"].get<int>();
  {
    lock_guard<mutex> lock(connMutex_);
    auto it = userConnMap_.find(toid);
    if (it != userConnMap_.end()) {
      it->second->send(js.dump());
      return;
    }
  }

  User user = userModel_.query(toid);
  if (user.getState() == "online") {
    redis_.publish(toid, js.dump());
    return;
  }
  // toid用户不存在，存储离线消息
  offlineMessageModel_.insert(toid, js.dump());
}

void ChatService::addFriend(const TcpConnectionPtr &conn, json &js,
                            Timestamp time) {
  int userid = js["id"].get<int>();
  int friendid = js["friendid"].get<int>();

  friendModel_.insert(userid, friendid);
}

void ChatService::createGroup(const TcpConnectionPtr &conn, json &js,
                              Timestamp time) {
  int userid = js["id"].get<int>();
  string name = js["groupname"];
  string desc = js["groupdesc"];
  
  Group group(-1, name, desc);
  if (groupModel_.createGroup(group)) {
    groupModel_.addGroup(userid, group.getId(), "creator");
  }
}

void ChatService::addGroup(const TcpConnectionPtr &conn, json &js,
                           Timestamp time) {
  int userid = js["id"].get<int>();
  int groupid = js["groupid"].get<int>();
  groupModel_.addGroup(userid, groupid, "normal");
}

void ChatService::groupChat(const TcpConnectionPtr &conn, json &js,
                            Timestamp time) {
  int userid = js["id"].get<int>();
  int groupid = js["groupid"].get<int>();
  vector<int> useridVec = groupModel_.queryGroupUsers(userid, groupid);

  lock_guard<mutex> lock(connMutex_);
  for (int id : useridVec) {
    auto it = userConnMap_.find(id);
    if (it != userConnMap_.end()) {
      it->second->send(js.dump());
    } else {
      User user = userModel_.query(id);
      if (user.getState() == "online") {
        redis_.publish(id, js.dump());
      } else {
        offlineMessageModel_.insert(id, js.dump());
      }
    }
  }
}

void ChatService::loginout(const TcpConnectionPtr &conn, json &js,
                           Timestamp time) {
  int userid = js["id"].get<int>();

  {
    lock_guard<mutex> lock(connMutex_);
    auto it = userConnMap_.find(userid);
    if (it != userConnMap_.end()) {
      userConnMap_.erase(it);
    }
  }

  redis_.unsubscribe(userid);

  User user(userid, "", "", "offline");
  userModel_.updateState(user);
}

void ChatService::handleRedisSubscribeMessage(int userid, string msg) {
  lock_guard<mutex> lock(connMutex_);
  auto it = userConnMap_.find(userid);
  if (it != userConnMap_.end()) {
    it->second->send(msg);
    return;
  }

  // 存储该用户的离线消息
  offlineMessageModel_.insert(userid, msg);
}