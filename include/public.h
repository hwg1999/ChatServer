#ifndef PUBLIC_H
#define PUBLIC_H

enum EnMsgType {
  LOGIN_MSG,     // 登录 
  LOGIN_MSG_ACK, // 登录确认
  LOGINOUT_MSG,  // 退出登录
  REG_MSG,       // 注册
  REG_MSG_ACK,   // 注册确认
  ONE_CHAT_MSG,  // 私聊
  ADD_FRIEND_MSG, // 添加好友
  CREATE_GROUP_MSG, // 创建群组
  ADD_GROUP_MSG,    // 加群
  GROUP_CHAT_MSG,   // 群聊
};

#endif