## Context

Phase 1 是纯 Echo 服务器，客户端发送任意文本，服务器原样返回。Phase 2 要引入结构化通信：客户端和服务器之间通过固定格式的 `struct Protocol` 交换信令。参考代码使用 C 风格实现，我们迁移到 C++ 时保持 wire format 不变，但用 C++ 惯用方式管理数据和逻辑。

**数据流：**
```
Client                          Server
  │                               │
  │  ── struct Protocol ──────▶   │  recv → switch(cmd)
  │                               │    REGISTE → find/add user
  │  ◀── struct Protocol ────     │    LOGIN   → verify credentials
  │                               │
```

## Goals / Non-Goals

**Goals:**
- 客户端可注册新账号（用户名+密码），服务器拒绝重复注册
- 客户端可登录已有账号，服务器验证用户名+密码
- 同一账号不能重复登录（已在线检测）
- 服务器维护最多 64 个用户的在线列表
- 客户端登录前后显示不同菜单界面
- 使用 `struct Protocol` 二进制协议通信

**Non-Goals:**
- 公聊/私聊/在线用户列表命令（Phase 3+）
- 密码加密存储
- 持久化存储（重启丢失）
- 登出通知其他用户（Phase 3 实现广播）

## Decisions

| 决策 | 选择 | 原因 |
|------|------|------|
| 协议结构体 | POD `struct Protocol`，固定大小 char 数组 | 二进制 send/recv 兼容，与参考代码一致 |
| 用户存储 | `std::array<OnlineUser, 64>` | 固定上限，无动态分配 |
| 命令分发 | `switch(msg.cmd)` | 直观，后续易扩展 |
| 客户端菜单 | 数字选择 + `std::cin` | 简单交互，无需 ncurses |
| 线程模型 | 保持不变：每客户端一个 `std::thread` | Phase 1 已验证可行 |

**协议结构体设计：**
```cpp
struct Protocol {
    int cmd;        // 命令类型 (REGISTE, LOGIN, ...)
    int state;      // 返回状态码 (OP_OK, NAME_EXIST, ...)
    char name[32];  // 用户名
    char data[64];  // 密码 或 消息数据
};
```

**服务器处理流程：**
```
recv Protocol → switch(cmd):
  REGISTE → find_user(name)
             found    → return NAME_EXIST
             not found → add_user(), return OP_OK

  LOGIN → find_user(name, passwd)
           not found → return NAME_PWD_NMATCH
           found     → check online?
                        online → return USER_LOGED
                        offline → set fd, return OP_OK

  (BROADCAST, PRIVATE, ONLINEUSER → Phase 3 stubs)
```

## Risks / Trade-offs

- **固定用户上限 64** → 仅学习项目，无需动态扩容
- **明文密码** → 后续 Phase 可加 hash
- **无消息边界处理** → `read/recv` 一次读整个 `struct Protocol`，sizeof 固定，不会半包
- **`system("clear")` 不跨平台** → 仅 Linux 目标
