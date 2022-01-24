#pragma once

#include <sys/syscall.h>
#include <unistd.h>

namespace CurrentThread {
extern __thread int t_cachedTid;

void cacheTid();

inline int tid() {
  // 将最有可能执行的分支告诉编译器, 执行if的可能性小
  if (__builtin_expect(t_cachedTid == 0, 0)) {
    cacheTid();
  }
  return t_cachedTid;
}
} // namespace CurrentThread