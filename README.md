# ChatRoom — C++ TCP 聊天室

基于 C++17 的多线程 TCP 聊天室，支持注册/登录、公聊、私聊、在线用户列表，含完整的心跳超时和错误传播机制。

## 快速开始

```bash
# 编译
cmake -B build && cmake --build build

# 启动服务器
./build/server 8888

# 启动客户端（另一个终端）
./build/client 127.0.0.1 8888
```

## 功能

| 功能 | 说明 |
|------|------|
| 用户注册/登录 | 用户名+密码，服务器内存存储（最多 64 用户） |
| 公聊 | 消息广播给所有在线用户 |
| 私聊 | 向指定用户发送消息 |
| 在线用户列表 | 查看当前在线用户 |
| 心跳超时检测 | 15s PING / 30s 超时强制离线 |
| 优雅关闭 | SIGINT 通知所有用户后有序退出 |
| 上线/离线通知 | 系统消息通知所有在线用户 |

## 架构

```
┌─────────────────────────────────────────────────┐
│ Server                                          │
│                                                 │
│  main()                                         │
│    ├── accept 循环 ────▶ client_handler 线程    │
│    │                    (每客户端一个)            │
│    │                       │                    │
│    ├── watchdog 线程       ├── recv Protocol    │
│    │   (心跳检测)           ├── dispatch 命令     │
│    │                       └── send 响应        │
│    └── 信号处理 (SIGINT)                        │
│                                                 │
│  online[64] — std::shared_mutex 保护            │
│    fd | flag | name | passwd | last_active      │
└─────────────────────────────────────────────────┘

  Protocol: 44B 固定头部 + 可变长度载荷 (TLV)
  ┌──────┬───────┬─────────┬─────────┐ ┌──────────┐
  │ cmd  │ state │data_len │ name    │ │ payload  │
  │ 4B   │ 4B    │ 4B      │ 32B     │ │ N bytes   │
  └──────┴───────┴─────────┴─────────┘ └──────────┘
```

## 采用的技术与知识点

### 1. RAII 资源管理 (`socket.h`)

```
知识点: RAII (Resource Acquisition Is Initialization)
```

**为什么采用**：socket fd 是操作系统资源，忘记 close 会导致 fd 泄漏。C 风格的手动 `close()` 分散在多个退出路径中极易遗漏。通过 `ScopedSocket` 将 fd 生命周期绑定到对象作用域，析构函数自动关闭，保证无泄漏。

```cpp
ScopedSocket sock(socket(...));  // 构造时获取
// ... 无论正常返回还是异常退出 ...
// ~ScopedSocket() 自动 close
```

`release()` 方法支持所有权显式转移，避免双重 close。

### 2. 长度前缀分帧协议 (`protocol.h`)

```
知识点: TCP 流式传输、消息边界分帧、TLV 编码
```

**为什么采用**：TCP 是字节流，不保留消息边界。多个 `send()` 可能被合并到一个 TCP 段，单个 `send()` 也可能被拆分。长度前缀分帧明确告诉接收端"这条消息有多长"，接收端循环 `recv` 直到收满指定字节数。

这是 **gRPC、WebSocket、Redis RESP、MySQL** 等工业协议的通用做法。

```
发送: send(header, 44B) + send(payload, data_len)
接收: recv_all(header, 44B) → 拿到 data_len → recv_all(payload, data_len)
```

`recv_all()` 循环读取处理 TCP 粘包/拆包：

### 3. 多线程并发模型 (`server.cpp`)

```
知识点: std::thread, detach(), 线程安全, std::shared_mutex
```

**为什么采用**：聊天室需要同时服务多个客户端。简单的一客户端一线程模型适合学习阶段，每个 `client_handler` 独立运行，代码清晰。

并发安全通过读写锁实现：

| 操作 | 锁类型 | 原因 |
|------|--------|------|
| 广播遍历 | `shared_lock` | 多读者并发 |
| 注册/登录 | `unique_lock` | 独占写入 |
| 标记离线 | `unique_lock` | 修改 fd |

**关键原则**：锁内不做 I/O。所有 `send_message()` 调用在锁外执行。

### 4. 条件变量替代忙等待 (`client.cpp`)

```
知识点: std::condition_variable, wait/notify 模式
```

**为什么采用**：`recv_loop` 在用户登录前需要等待。最初的实现用 `sleep_for(200ms)` 轮询 `login_f`——白白消耗 CPU。条件变量让线程真正休眠，登录成功时 `notify_one()` 精确唤醒：

```cpp
// 等待端
login_cv.wait(lock, [] { return login_f == 1; });

// 通知端
login_f = 1;
login_cv.notify_one();
```

### 5. 原子操作 (`protocol.h`, `server.cpp`)

```
知识点: std::atomic<T>, lock-free 编程
```

**为什么采用**：`last_active` 时间戳被多个线程频繁读取（watchdog 每 5 秒检查 64 个槽位），用 `std::atomic<time_t>` 保证原子读写，避免加锁开销。

### 6. 心跳超时检测 (`server.cpp`)

```
知识点: watchdog 模式、定时检测、PING/PONG 协议
```

**为什么采用**：TCP 连接断开时，如果没有数据传输，`recv()` 可能永远阻塞无法感知对端断开（例如拔网线、路由器宕机等不发送 FIN 的场景）。心跳机制主动探测：

- 15 秒空闲 → 发 PING
- 客户端自动回复 PONG（对用户透明）
- 30 秒无 PONG → 判定死亡，强制清理

### 7. 信号驱动的优雅关闭 (`server.cpp`)

```
知识点: signal(), volatile sig_atomic_t, 信号安全函数
```

**为什么采用**：Ctrl+C 直接杀进程会导致用户收不到通知、socket 残留。信号处理函数关闭监听 socket 使 `accept()` 返回错误，主循环退出后广播"服务器正在关闭"、关闭所有连接、有序退出。

### 8. 错误传播 (`protocol.h`, `server.cpp`)

```
知识点: 防御式编程、错误码传播、自动资源清理
```

**为什么采用**：`send_message()` 返回 `bool` 而非 `void`，调用方可感知发送失败。广播时 send 失败立即 `close` + `del_user_online`，在秒级清理死连接，无需等待 watchdog 30 秒超时。

### 9. C++17 特性

| 特性 | 使用位置 | 说明 |
|------|----------|------|
| `constexpr` | `protocol.h` | 编译期常量替代 `#define`，类型安全 |
| `std::array` | `server.cpp` | 固定大小数组，替代 C 数组，支持迭代器 |
| `std::string_view` | — | 可用但未强制引入，消息传递用 `std::string` |
| `std::shared_mutex` | `server.cpp` | 读写锁，C++17 标准库原生支持 |
| `std::atomic<T>` | `protocol.h` | 无锁原子类型 |
| Structured bindings | `server.cpp` | `for (auto& [fd, idx] : targets)` |

### 10. CMake 构建

```
知识点: CMakeLists.txt, target-based 构建
```

C++17 标准 + pthread 链接，两个独立 target（server/client），共享根目录头文件。

## 项目结构

```
chatroom/
├── CMakeLists.txt          # CMake 构建配置 (C++17)
├── socket.h                # ScopedSocket RAII 封装
├── protocol.h              # 通信协议 + I/O 工具函数
├── server.cpp              # 服务器 (~630 行)
├── client.cpp              # 客户端 (~410 行)
└── README.md
```

## 演进历史

从 C 语言参考代码出发，通过 7 个 OpenSpec 变更逐步构建：

1. **Phase 1** — C++ 化 Echo Server/Client
2. **Phase 2** — 结构化协议 + 注册/登录
3. **Phase 3** — 公聊、私聊、在线用户列表
4. **Variable-length protocol** — TLV 分帧，突破 63 字符限制
5. **Heartbeat timeout** — PING/PONG 心跳 + watchdog
6. **Error propagation** — send 失败检测 + 自动清理
7. **Refinement** — 并发安全、优雅关闭、condition_variable

## License

MIT
