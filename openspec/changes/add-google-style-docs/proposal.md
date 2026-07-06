## Why

Phase 1 代码几乎没有注释，`server.cpp` 和 `client.cpp` 的函数缺少参数说明、返回值描述和行为描述。随着后续 Phase 逐步叠加功能（广播、用户身份、多房间），代码量增长后无注释将变得难以维护。需要尽早建立注释规范。

## What Changes

- 确立 Google 风格注释规范作为项目标准，应用于所有 C++ 源码
- 头文件提供完整的类/接口文档
- 每个函数注释包含参数说明、返回值、功能描述
- 关键逻辑处添加行内注释
- 对现有代码（`socket.h`, `server.cpp`, `client.cpp`）进行注释改造

## Capabilities

### New Capabilities
- `code-documentation`: 定义项目的代码注释与文档标准

### Modified Capabilities
（无）

## Impact

- 修改文件：`socket.h`, `server.cpp`, `client.cpp`
- 新增规格：`specs/code-documentation/spec.md`
- 无运行时影响
