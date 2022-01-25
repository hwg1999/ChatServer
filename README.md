# ChatServer
集群聊天服务器

# 目录树
```
ChatServer
├── CMakeLists.txt
├── autobuild.sh
├── include     
│   ├── public.hpp
│   └── server
│       ├── chatserver.hpp
│       ├── chatservice.hpp
│       ├── db
│       ├── model
│       └── redis
├── src
│   ├── CMakeLists.txt
│   ├── client
│   └── server
│       ├── CMakeLists.txt
│       ├── chatserver.cpp
│       ├── chatservice.cpp
│       ├── db
│       ├── main.cpp
│       ├── model   操作数据库的model类实现
│       └── redis
└── thirdparty
    ├── json.hpp   第三方json库nlohmann
    └── miniMuduo  仿写的miniMuduo
        ├── base
        └── net
```
