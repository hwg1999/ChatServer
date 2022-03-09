#include "Logger.h"
#include "Timestamp.h"

#include <iostream>
#include <mutex>

std::mutex Logger::lock{};

// 设置日志级别
void Logger::setLogLevel(int level) { logLevel_ = level; }

// 写日志  [级别信息] time : msg
void Logger::log(const std::string &msg) {
  std::unique_lock<std::mutex> guard(lock);

  switch (logLevel_) {
    case INFO:
      std::cout << "[INFO]  ";
      break;
    case ERROR:
      std::cout << "[ERROR] ";
      break;
    case FATAL:
      std::cout << "[FATAL] ";
      break;
    case DEBUG:
      std::cout << "[DEBUG] ";
      break;
    default:
      break;
  }
  // 打印时间和msg

  std::cout << Timestamp::now().toString() << " : " << msg << std::endl;
}