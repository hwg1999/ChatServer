- [项目介绍](#项目介绍)
- [目录树](#目录树)
- [数据模块](#数据模块)
  - [表的设计](#表的设计)
  - [数据库模块设计](#数据库模块设计)
- [通信格式](#通信格式)
- [网络和业务模块](#网络和业务模块)
  - [网络模块](#网络模块)
  - [网络模块和业务模块解耦合](#网络模块和业务模块解耦合)
  - [业务模块](#业务模块)
    - [注册业务](#注册业务)
    - [登录业务](#登录业务)
    - [加好友业务](#加好友业务)
    - [一对一聊天业务](#一对一聊天业务)
    - [创建群业务](#创建群业务)
    - [加入群业务](#加入群业务)
    - [群聊业务](#群聊业务)
    - [注销业务](#注销业务)
- [服务器集群](#服务器集群)
- [跨服务器通信](#跨服务器通信)
- [项目遇到的问题](#项目遇到的问题)

# 项目介绍

项目主要是实现一个简易通讯工具，主要业务分为注册、登录、加好友、查看离线消息、一对一群聊、创建群、加入群、群聊等。

详细业务流程关系如下图：

![image-20220414102104972](asserts/image-20220414102104972.png)

# 目录树

```
ChatServer
├── CMakeLists.txt
├── autobuild.sh
├── include     // 头文件
├── src
│   ├── CMakeLists.txt
│   ├── client	// 客户端代码
│   └── server
│       ├── CMakeLists.txt
│       ├── chatserver.cpp
│       ├── chatservice.cpp	// 业务代码
│       ├── db		 // mysql连接、更新相关代码
│       ├── main.cpp
│       ├── model    // 操作数据库的model类实现
│       └── redis	 // 发布订阅相关代码
└── thirdparty
    ├── json.hpp   // 第三方json库nlohmann
    └── miniMuduo  // 仿写的miniMuduo
        ├── base
        └── net
```

# 数据模块

## 表的设计

**表User：**

作为一个聊天系统，服务器端肯定要有用户的信息，比如说账号，用户名，密码等。

在**登录**的时候，我们可以查询这个表里面的信息对用户身份进行验证，在**注册**的时候，我们则可以往表里面去写入数据。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210127144426745.png)

**表Friend：**

用户登录之后，首先就是进行聊天业务，我们必须要知道该用户的好友都有谁。
在**加好友**时，我们就可以往这张表里面去写入信息并在**一对一聊天**时查询这里面的信息去看好友是否在线。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210127144758137.png)

**表OfflineMessage：**

我们的设计目标里要存储离线消息，这就涉及到离线消息发给谁，谁发的，发的什么三个问题，所以我们又需要一个新表来存储离线消息。这样我们一旦有**离线消息**便可以往这个表里面去写入数据。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210127145215562.png)

**表AllGroup：**

然后便是群组业务了，群组中我们需要有一个记录群组信息的表，方便我们**创建群**时往其中去写入数据；

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210127145609263.png)

**表Group:**

同时群里面肯定是有群员的，我们就需要一个记录群成员的表，我们在**加入群**的时候，把用户id写入这个表。并且在**发送群消息**的时候查询这个表由服务器向这些成员转发消息。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210127150030734.png)

## 数据库模块设计

在`/include/server/db/db.h`文件中，封装着对数据库的连接、查询、更新、释放连接几个操作。

其是数据库模块中设计的最底层，为上层各个表以及其操作模块提供基础的服务。其关系图如下所示：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210127151155139.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3NoZW5taW5neHVlSVQ=,size_16,color_FFFFFF,t_70)

- 名字带有model的类都是对数据库的操作类并不负责存储数据，而像User这个类则是负责暂存从数据库中查询到的数据
- FriendModel和OfflineMessageModel则是没有暂存数据的上层类，这是因为对于Friend来说，其数据本身就是一个User，只需要查询到好友的id然后在User表中内联查询一下便可得到信息；对于OfflineMessage没有
- 这些类都在`/include/server/model`里面

# 通信格式

服务器和客户端的通信采用了JSON来完成数据在网络中的标准传输。

对于不同的数据则是采用了不同的格式，具体如下：

```cpp
1.登录
json["msgid"] = LOGIN_MSG;
json["id"]			//用户id
json["password"]	//密码

2.登录反馈
json["msgid"] = LOGIN_MSG_ACK;
json["id"]			//登录用户id
json["name"]		//登录用户密码
json["offlinemsg"]	//离线消息
json["friends"]		//好友信息,里面有id、name、state三个字段
json["groups"]		//群组信息,里面有id，groupname，groupdesc，users三个字段
					//users里面则有id，name，state，role四个字段
json["errno"]		//错误字段，正确登录时设置为0, 错误时被设置成1，用户不在线设置成2
json["errmsg"]		//错误信息

3.注册
json["msgid"] = REG_MSG;
json["name"]		//用户姓名
json["password"]	//用户姓名

4.注册反馈
json["msgid"] = REG_MSG_ACK;
json["id"]			//给用户返回他的id号
json["errno"]		//错误信息，失败会被设置为1

5.加好友
json["msgid"] = ADD_FRIEND_MSG;
json["id"]			//当前用户id
json["friendid"]	//要加的好友的id

6.一对一聊天
json["msgid"] = ONE_CHAT_MSG;
json["id"]			//发送者id
json["name"]		//发送者姓名
json["to"]			//接受者id
json["msg"]			//消息内容
json["time"]		//发送时间

7.创建群
json["msgid"] = CREATE_GROUP_MSG;
json["id"]			//群创建者id
json["groupname"]	//群名
json["groupdesc"]	//群描述

8.加入群
json["msgid"] = ADD_GROUP_MSG;
json["id"]			//用户id
json["groupid"]		//群id

9.群聊
json["msgid"] = GROUP_CHAT_MSG;
json["id"]			//发送者id
json["name"]		//发送者姓名
json["groupid"]		//发送者姓名
json["msg"]			//消息内容
json["time"]		//发送时间

10.注销
json["msgid"] = LOGINOUT_MSG;
json["id"]			//要注销的id
```



# 网络和业务模块

## 网络模块

这里使用了仿写的miniMuduo，只写了服务端的代码，其简要类图如下

![image-20220414103323339](asserts/image-20220414103323339.png)

miniMuduo使用的是oneloop per thread + threadpool模型，有一个main reactor负责接收来自客户端的连接。然后使用轮询的方式给sub reactor去分配连接，而客户端的读写事件都在这个sub reactor上进行。

![image](https://user-images.githubusercontent.com/63459176/157204919-b51e3623-ea3b-42e4-9a62-0cbb23f2d1ca.png)

这里提供了两个非常重要的注册回调接口：**连接回调**和**消息回调**

```cpp
//注册连接回调
server_.setConnectionCallback(bind(&ChatServer::on_connection, this, _1));

//注册消息回调
server_.setMessageCallback(bind(&ChatServer::on_message, this, _1, _2, _3));
```

在这里设置一个处理有关连接事件的方法和处理读写事件的方法。

```cpp
//上报连接相关信息的回调函数
void OnConnection(const TcpConnectionPtr &);

//上报读写时间相关信息的回调函数
void OnMessage(const TcpConnectionPtr &, Buffer *, Timestamp);
```

当用户进行连接或者断开连接时便会调用OnConnection方法进行处理，其执行对象应该是main reactor。

发生读写事件时，则会调用OnMessage方法，执行对象为sub reactor。

## 网络模块和业务模块解耦合

在**通信模块**中，有一个字段`msgid`，其代表着服务器和客户端通信的消息类型，值是一个枚举类型，保存在`/include/public.hpp`文件中，总共有10个取值：

```cpp
    LOGIN_MSG = 1,  //登录消息，
    LOGIN_MSG_ACK,  //登录响应消息
    REG_MSG,        //注册消息
    REG_MSG_ACK,    //注册响应消息
    ONE_CHAT_MSG,   //一对一聊天消息
    ADD_FRIEND_MSG, //添加好友消息

    CREATE_GROUP_MSG, //创建群聊
    ADD_GROUP_MSG,    //加入群聊
    GROUP_CHAT_MSG,   //群聊消息

    LOGINOUT_MSG,   //注销消息
```

根据这些消息类型，我们可以在业务模块添加一个无序关联容器unordered_map，其间为消息类型，值为发生不同类型事件所应该调用方法。

```cpp
msg_handler_map_.insert({LOGIN_MSG, bind(&ChatService::login, this, _1, _2, _3)});
msg_handler_map_.insert({LOGINOUT_MSG, bind(&ChatService::loginout, this, _1, _2, _3)});
msg_handler_map_.insert({REG_MSG, bind(&ChatService::register, this, _1, _2, _3)});
msg_handler_map_.insert({ONE_CHAT_MSG, bind(&ChatService::onechat, this, _1, _2, _3)});
msg_handler_map_.insert({ADD_FRIEND_MSG, bind(&ChatService::addfriend, this, _1, _2, _3)});
msg_handler_map_.insert({CREATE_GROUP_MSG, bind(&ChatService::creategroup, this, _1, _2, _3)});
msg_handler_map_.insert({ADD_GROUP_MSG, bind(&ChatService::addgroup, this, _1, _2, _3)});
msg_handler_map_.insert({GROUP_CHAT_MSG, bind(&ChatService::groupchat, this, _1, _2, _3)});
```

这样我们就得到了一个存储消息类型和处理这个消息的方法的容器。

这样，业务模块只需根据相应的消息类型调用相应的函数，专注于业务处理。

```cpp
//解耦网络和业务模块的代码
//通过js里面的msgid，绑定msgid的回调函数，获取业务处理器handler
auto msg_handler = ChatService::instance()->get_handler(js["msgid"].get<int>());

msg_handler(conn, js, time);
```

## 业务模块

由于与网络模块的解耦，在业务模块我们就不用去关系网络上所发生的事情，专注业务便可。

具体业务相关的数据结构如下：

```cpp
//存储在线用户的连接情况，便于服务器给用户发消息，注意线程安全
unordered_map<int, TcpConnectionPtr> user_connection_map_;

mutex conn_mutex_;

UserModel user_model_;
OfflineMessageModel offline_message_model_;
FriendModel friend_model_;
GroupModel group_model_;
```

### 注册业务

当服务器接收到 json 字符串的时候，对其进行反序列化，得到要注册的信息，然后写入到User表中，成功就将id号返回，失败就把errno字段设置为1。

### 登录业务

当服务器接收到 json 字符串的时候，对其进行反序列化，得到用户传递过来的账号和密码信息。

首先就是检测这个账号和密码是否与服务器中的数据匹配，如果不匹配就把errno设置为1并返回 id or password error的错误信息。

如果匹配，就检测当前用户是否在线，因为有在别的设备登录的状况，如果在线就把errno设置为2，返回 id is online的错误信息

如果用户不在线，这个时候用户就是**登陆成功**了，这个时候服务器就把该用户的**好友列表，群组列表**以及**离线消息**都推送给该用户。

### 加好友业务

这个业务很简单，服务器得到反序列化的信息，然后将这个信息写入Friend表中即可。

### 一对一聊天业务

服务器接收到客户端的信息，然后去本服务器的`user_connection_map_`接受信息的用户是否在本服务器在线，在线的话直接转发即可，不在线的话，看看数据库里面的信息是否是在线，如果在线，那么就是接收用户在其他服务器登录，将消息通过redis中间件转发即可。

如果均不在线，转储离线消息即可。

### 创建群业务

服务器接收到客户端的信息，把群组的信息写入到AllGroup表中，并将创建者的信息写入到GroupUser中，设置创建者为creator

### 加入群业务

服务器接收到客户端的信息，将用户数据写入到GroupUser表中，并将role角色设置为normal。

### 群聊业务

服务器接收到客户端的信息，先去GroupUser查询到所有群员的id，然后一个个去本服务器的`user_connection_map_`接受信息的用户是否在本服务器在线，在线的话直接转发即可，不在线的话，看看数据库里面的信息是否是在线，如果在线，那么就是接收用户在其他服务器登录，将消息通过redis中间件转发即可。

如果均不在线，转储离线消息即可。

### 注销业务

服务器收到客户端发来的信息，将该用户在User表中所对应的state改为offline。

# 服务器集群

一般来说，一台服务器只能支持1~2w的并发量，但是这是远远不够的，我们需要更高的并发量支持，这个时候就需要引入Nginx tcp长连接负载均衡算法。

当一个新的客户端连接到来时，负载均衡模块便会根据我们在nginx.conf里面设置的参数来分配服务器资源。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210127200450432.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3NoZW5taW5neHVlSVQ=,size_16,color_FFFFFF,t_70)

按图中所示，客户端只用连接我们的负载均衡服务器，然后服务器就会自动把client连接分配到对应的server服务器。

# 跨服务器通信

如果一个在server1的用户想要给在server2的用户发送消息要怎么办呢？

是像下面这样让服务器与服务器建立连接？

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210127201004249.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3NoZW5taW5neHVlSVQ=,size_16,color_FFFFFF,t_70)
这样肯定不行，服务器之间关联性太强了，一旦多加一个服务器，以前的服务器都要增加一条指向它的连接。

所以，我们可以借鉴交换机连接PC的思想，引入Redis消息队列。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210127201649999.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3NoZW5taW5neHVlSVQ=,size_16,color_FFFFFF,t_70)

当客户端登录的时候，服务器吧它的id号 subscribe到redis中间件，表示该服务器对这个id发生的事件感兴趣，而Redis收到发送给该id的消息时就会 把消息转发到这个服务器上。

# 项目遇到的问题

1、客户端和服务端用Ctrl+C异常退出导致数据库没有更新登录状态

*   **为客户端关闭连接注册一个事件，在关闭连接时更新数据库状态**

*   **服务端则为SIGINT信号注册一个处理函数，在服务器Ctrl+C退出时将所有用户状态更新为离线**

2、离线消息表userid为主键导致用户的离线消息覆盖

- **将其设置为非主键**

3、客户端的两个线程分别负责接收和发送，但是主线程却又负责接收解析关于登录成功与否的消息，结果是子线程接收到了本该由主线程解析的消息，然后子线程并不解析，导致两个线程均阻塞在recv上

- **职责划分彻底，消息接收完全由子线程负责，设置一个信号量来同步，待子线程解析完毕后主线程才往下走，显示主界面。**

**4、**客户端调用send时strlen(buffer.c\_str()) + 1写成了strlen(buffer.c\_str() + 1)导致json解析错误

- **通过LOG定位到错误（一开始cout发现生成的json没问题，逐步定位到send处发现问题）**

5、网络库里accept出现错误

- **通过日志得知错误号22，发现是参数错误，原来是socklen没有初始化**

6、向消息中间件PUBLISH发布消息不成功，导致无法跨服务器通信

- **订阅和发布要使用不同上下文**

7、多次向中间件SUBSCRIBE订阅消息，出现无响应，客户端也无响应

**有专门用一个线程来接收订阅消息的通知，但是一开始在主线程在subscribe里用的是redisCommand，这个函数相当于调用了三个函数**

1.  **redisAppendCommand 把命令写入本地发送缓冲区**

2.  **redisBufferWrite 把本地缓冲区的命令通过网络发送出去**

3.  **redisGetReply 阻塞等待redis server响应消息**

**也就是说接收线程抢掉了订阅消息，然后主线程一直阻塞在redisGetReply，无法回到epoll的流程了**

**既然如此，主线程subscribe的时候用redisAppendCommand和redisBufferWrite就行了，由接收线程负责接收消息。**
