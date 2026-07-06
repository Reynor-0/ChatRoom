## Context

从 C 语言的参考代码出发，用现代 C++ 重写。目标是功能一致（Echo），但代码结构更安全、更清晰。项目采用单文件结构，Server 和 Client 各自独立编译。

**环境（已验证）：**
- g++ 11.4.0 (Ubuntu 22.04) — 完整支持 C++17
- CMake 3.22.1
- GNU Make 4.3
- Linux 6.8

## Goals / Non-Goals

**Goals:**
- RAII 管理 socket fd，自动关闭
- `std::thread` + lambda 处理客户端连接
- `std::string` / `std::getline` 处理消息
- CMake 构建系统
- 功能等同学参考代码（Echo）

**Non-Goals:**
- 消息广播（Phase 2）
- 用户身份/昵称（Phase 3）
- select/epoll 多路复用
- 异常安全/超时控制
- 跨平台（仅 Linux）

## Decisions

| 决策 | 选择 | 原因 |
|------|------|------|
| Socket 封装 | 手写轻量 `ScopedSocket` | 不引入库依赖，10 行代码解决 |
| 线程模型 | `std::thread` + detach | Phase 1 最简单方案，Phase 3 再引入线程池 |
| 消息分界 | 按 `recv` 自然边界 | 不引入协议，Phase 1 简单即可 |
| 构建 | CMake 3.10+ | 后续扩展方便 |
| 标准 | C++17 | `std::string_view`、`if constexpr` 等 |

**ScopedSocket 设计：**
```
class ScopedSocket {
    int fd_;
public:
    explicit ScopedSocket(int fd) : fd_(fd) {}
    ~ScopedSocket() { if (fd_ >= 0) close(fd_); }
    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;
    int get() const { return fd_; }
};
```

## Risk / Trade-offs

- **`std::thread::detach` 无法 join** → Phase 3 改用线程池或 `std::jthread` (C++20)
- **无消息边界协议** → 当前 `printf` + `send` 足够，后续引入换行分隔
- **`recv` 可能收到不完整的行** → 在当前 Echo 场景下不影响（收到什么就回显什么）

## Open Questions

- 是否需要引入 spdlog/fmt 等日志库？（当前用 `std::cout` 足够）
