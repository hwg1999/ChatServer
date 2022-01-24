#pragma once

#include <string>

#include "noncopyable.hpp"

// 用于粗粒度的程序运行过程记录.
#define LOG_INFO(logmsgFormat, ...)                                            \
  do {                                                                         \
    Logger logger;                                                             \
    logger.setLogLevel(INFO);                                                  \
    char buf[1024] = {0};                                                      \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                          \
    logger.log(buf);                                                           \
  } while (0)

// 出错但不影响程序运行的事件记录.
#define LOG_ERROR(logmsgFormat, ...)                                           \
  do {                                                                         \
    Logger logger;                                                             \
    logger.setLogLevel(ERROR);                                                 \
    char buf[1024] = {0};                                                      \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                          \
    logger.log(buf);                                                           \
  } while (0)

// 用于调试的细粒度记录. 只有在Debug编译模式下会输出.
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                                           \
  do {                                                                         \
    Logger logger;                                                             \
    logger.setLogLevel(DEBUG);                                                 \
    char buf[1024] = {0};                                                      \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                          \
    logger.log(buf);                                                           \
  } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

// 出错影响程序继续进行, 记录并终止进程.
#define LOG_FATAL(logmsgFormat, ...)                                           \
  do {                                                                         \
    Logger logger;                                                             \
    logger.setLogLevel(FATAL);                                                 \
    char buf[1024] = {0};                                                      \
    snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);                          \
    logger.log(buf);                                                           \
    exit(-1);                                                                  \
  } while (0)

// 日志的级别
enum LogLevel {
  INFO,  // 普通信息
  ERROR, // 错误信息
  DEBUG, // 调试信息
  FATAL, // core信息
};

// 输出一个日志类
class Logger : noncopyable {
public:
  // 设置日志级别
  void setLogLevel(int level);
  // 写日志
  void log(const std::string &msg);

private:
  int logLevel_;
};