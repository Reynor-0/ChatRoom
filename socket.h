//
// socket.h
// POSIX socket 文件描述符的 RAII 封装。
//
// 用法：
//   #include "socket.h"
//   ScopedSocket sock(socket(AF_INET, SOCK_STREAM, 0));
//   // ... 通过 sock.get() 获取原始 fd 进行系统调用 ...
//   // sock 离开作用域时自动关闭 fd
//

#pragma once

#include <unistd.h>

//
// @brief  POSIX socket 文件描述符的 RAII 封装。
//
// 在析构时自动关闭底层 fd。不可拷贝、不可移动 — 所有权唯一。
//
class ScopedSocket {
public:
    //
    // @brief  构造 wrapper，接管 @p fd 的所有权。
    // @param  fd  一个有效的 socket 文件描述符（-1 表示空 wrapper）。
    //
    explicit ScopedSocket(int fd) : fd_(fd) {}

    //
    // @brief  如果持有的 fd 有效（>= 0），自动调用 close。
    //
    ~ScopedSocket() {
        if (fd_ >= 0)
            close(fd_);
    }

    // 不可拷贝 — 每个 ScopedSocket 唯一拥有自己的 fd。
    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    //
    // @brief  返回原始文件描述符。
    // @return 持有的 fd（未初始化时为 -1）。
    //
    int get() const { return fd_; }

private:
    int fd_;
};
