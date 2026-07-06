## Context

现有的 `server.cpp`、`client.cpp`、`socket.h` 仅有少量注释（1-2 处行内注释），缺乏系统性的函数文档。需要在不大幅重构代码的前提下，为所有函数/类添加 Google 风格注释。

## Goals / Non-Goals

**Goals:**
- 每个函数/方法前有 Google 风格注释块（`@param`, `@return`, `@brief`）
- 类/结构体有简要功能描述
- 关键逻辑处（accept 循环、recv/send、线程创建等）有行内注释
- 文件头部有模块说明

**Non-Goals:**
- 不引入 doxygen 构建流程（注释格式兼容即可）
- 不修改代码逻辑

## Decisions

**注释格式**（Google C++ Style）:
```
//
// @brief  brief description of the function
//
// @param  name  description of the parameter
// @return       description of return value
//
```

对于简单成员函数（1-2 行 getter/setter），可用单行 `///` 注释。

**文件头格式**:
```
//
// filename.ext
// Brief description of what this file contains.
//
// Usage:
//   compile: g++ ...
//   run:     ./program args
//
```

## Risks / Trade-offs

- 注释与代码不同步的风险 → 后续 code review 中检查
- 过度注释简单函数的风险 → 仅对公开接口/复杂逻辑强制要求
