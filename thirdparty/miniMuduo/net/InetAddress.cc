#include "InetAddress.hpp"

#include <string.h>
#include <strings.h>

InetAddress::InetAddress(const std::string &ip, uint16_t port) {
  bzero(&addr_, sizeof addr_);
  addr_.sin_family = AF_INET;
  addr_.sin_port = htons(port);
  addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

// 返回ip地址的点分十进制字符串
std::string InetAddress::toIp() const {
  char buf[64] = {0};
  ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
  return buf;
}

// ip地址:端口号
std::string InetAddress::toIpPort() const {
  char buf[64] = {0};
  ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
  size_t end = strlen(buf);
  uint16_t port = ntohs(addr_.sin_port);
  sprintf(buf + end, ":%u", port);
  return buf;
}

uint16_t InetAddress::toPort() const { return ntohs(addr_.sin_port); }