cmake_minimum_required(VERSION 3.0)
project(chat)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/server)
include_directories(${PROJECT_SOURCE_DIR}/include/server/db)
include_directories(${PROJECT_SOURCE_DIR}/include/server/model)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)
include_directories(${PROJECT_SOURCE_DIR}/include/server/redis)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/miniMuduo/base)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/miniMuduo/net)

# 定义参与编译的源代码文件
aux_source_directory(${PROJECT_SOURCE_DIR}/thirdparty/miniMuduo/base SRC_BASE)
aux_source_directory(${PROJECT_SOURCE_DIR}/thirdparty/miniMuduo/net SRC_NET)
# 编译生成动态库mymuduo
add_library(muduo_base SHARED ${SRC_BASE})
add_library(muduo_net SHARED ${SRC_NET})

add_subdirectory(src)