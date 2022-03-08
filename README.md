# ChatServer
集群聊天服务器

# 目录树
```
ChatServer
├── CMakeLists.txt
├── autobuild.sh
├── include     
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

# miniMuduo
miniMuduo采用的是oneloop per thread + threadpool模型
![image](https://user-images.githubusercontent.com/63459176/157204919-b51e3623-ea3b-42e4-9a62-0cbb23f2d1ca.png)

网络部分的简化类图
![image](https://user-images.githubusercontent.com/63459176/157203161-87827f3a-f857-42dd-a443-f74baa061797.png)

客户端建立连接的时序图
![image](https://user-images.githubusercontent.com/63459176/157204353-2268b872-5dc7-4987-be83-8a99488036fa.png)

客户端断开连接的时序图
![image](https://user-images.githubusercontent.com/63459176/157204439-47eac7a2-d916-40a3-b02c-1de30e6e2fcd.png)


