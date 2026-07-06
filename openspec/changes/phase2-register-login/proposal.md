## Why

Phase 1 仅实现了 Echo 回显，每个客户端独立通信，没有用户身份概念。要实现聊天室功能，必须先建立用户体系 — 注册账号、登录认证、在线状态管理。参考代码已提供了完整的注册/登录信令流程，需要将其移植到 C++ 版本。

## What Changes

- 新增 `protocol.h`：定义通信协议结构体、命令码、返回码、在线用户结构
- 改造 `server.cpp`：维护在线用户数组，实现注册/登录处理流程，按 cmd 分发消息
- 改造 `client.cpp`：增加菜单界面，支持注册/登录交互，登录前后显示不同菜单
- 消息传输从文本流改为结构体二进制传输

## Capabilities

### New Capabilities
- `auth-system`: 用户注册与登录认证，服务器端维护用户名/密码/在线状态
- `client-menu`: 客户端交互式菜单，登录前后展示不同功能入口

### Modified Capabilities
（无，Phase 1 的 echo-server/echo-client 功能被 auth-system 替代）

## Impact

- 新增：`protocol.h`
- 修改：`server.cpp`, `client.cpp`, `CMakeLists.txt`（无需新增 target）
- **BREAKING**: 通信协议从纯文本改为 `struct Protocol` 二进制格式，与 Phase 1 不兼容
