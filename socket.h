//
// socket.h
// RAII wrapper for POSIX socket file descriptors.
//
// Usage:
//   #include "socket.h"
//   ScopedSocket sock(socket(AF_INET, SOCK_STREAM, 0));
//   // ... use sock.get() for system calls ...
//   // fd is automatically closed when sock goes out of scope
//

#pragma once

#include <unistd.h>

//
// @brief  RAII wrapper that owns a POSIX socket file descriptor.
//
// Automatically closes the underlying fd in the destructor.
// Non-copyable and non-movable — ownership is unique.
//
class ScopedSocket {
public:
    //
    // @brief  Constructs the wrapper, taking ownership of @p fd.
    // @param  fd  A valid socket file descriptor (or -1 for an empty wrapper).
    //
    explicit ScopedSocket(int fd) : fd_(fd) {}

    //
    // @brief  Closes the owned file descriptor if it is valid (>= 0).
    //
    ~ScopedSocket() {
        if (fd_ >= 0)
            close(fd_);
    }

    // Non-copyable — each ScopedSocket uniquely owns its fd.
    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;

    //
    // @brief  Returns the raw file descriptor.
    // @return The managed fd (may be -1 if uninitialized).
    //
    int get() const { return fd_; }

private:
    int fd_;
};
