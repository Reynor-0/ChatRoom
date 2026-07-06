## Why

参考代码是 C 风格的多线程 Echo Server，存在裸指针泄漏、`pthread` 手动管理、`scanf` 空格截断等问题。需要一个 C++17 风格的基础版本作为聊天室项目的起点，之后逐步叠加功能。

## What Changes

- 用 C++17 + CMake 重写参考代码的 Echo Server/Client 功能
- 使用 RAII 封装 Socket 生命周期，避免资源泄漏
- 用 `std::thread` 替代 `pthread`，`std::string` 替代 `char[]`
- Client 端用 `std::getline` 替代 `scanf`，支持含空格的消息
- 消息行为保持 Echo（单播回显），广播留到 Phase 2

## Capabilities

### New Capabilities
- `echo-server`: TCP Echo Server，监听端口，accept 连接，每个客户端一个线程回显消息
- `echo-client`: TCP Echo Client，连接服务器，发送 stdin 输入，接收并打印回显

### Modified Capabilities
（无，这是全新项目）

## Impact

- 新增文件：`server.cpp`, `client.cpp`, `CMakeLists.txt`
- 依赖：C++17, pthread, CMake >= 3.10
