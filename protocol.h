//
// protocol.h
// 服务器与客户端之间的通信协议定义。
// 采用长度前缀分帧：固定大小头部 + 可变长度载荷。
//
// 用法：
//   #include "protocol.h"
//   server.cpp 和 client.cpp 共同包含此头文件。
//
// 发送：
//   Protocol hdr{cmd, 0, len, ...};
//   send_message(fd, hdr, payload_ptr);
//
// 接收：
//   Protocol hdr;
//   std::string data = recv_message(fd, hdr);
//

#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>

#include <sys/socket.h>

// ---------- 协议常量 ----------

constexpr int SERVER_PORT = 8888;
constexpr int MAX_USER_NUM = 64;
constexpr int MAX_DATA_LEN = 65536;   // 单条消息最大 64KB，防止恶意超大包

// ---------- 心跳超时参数 ----------

constexpr int PING_INTERVAL = 15;     // 空闲 15 秒后发送 PING
constexpr int TIMEOUT       = 30;     // 30 秒无活动判定为死连接
constexpr int WATCHDOG_TICK =  5;     // watchdog 每 5 秒检查一轮

// ---------- 在线用户记录（服务器端） ----------

//
// @brief  服务器端用户记录。
//
// 同时跟踪用户的注册信息和当前在线状态。
//
struct OnlineUser {
    int  fd;              // socket fd，-1 表示离线
    int  flag;            // -1：空槽位，1：已有注册数据
    char name[32];        // 用户名
    char passwd[32];      // 密码（当前为明文）
    std::atomic<time_t> last_active{0};  // 最近活跃时间戳
};

// ---------- 通信协议结构体（长度前缀分帧） ----------

//
// @brief  消息头部（固定 44 字节）。
//
// 采用长度前缀分帧：头部后紧跟 data_len 字节的载荷数据。
// 接收端先读头部获取 data_len，再按长度读取载荷。
//
struct Protocol {
    int  cmd;             // 命令类型（见下方命令码）
    int  state;           // 返回/状态码
    int  data_len;        // 载荷长度（字节数，0 表示无载荷）
    char name[32];        // 用户名（或目标用户名）
};

// ---------- 命令码（客户端 → 服务器） ----------

constexpr int BROADCAST   = 0x00000001;  // 广播消息给所有用户
constexpr int PRIVATE     = 0x00000002;  // 发送私聊消息给指定用户
constexpr int REGISTE     = 0x00000004;  // 注册新账号
constexpr int LOGIN       = 0x00000008;  // 登录
constexpr int ONLINEUSER  = 0x00000010;  // 列出在线用户
constexpr int LOGOUT      = 0x00000020;  // 登出/断开连接
constexpr int HEARTBEAT   = 0x00000040;  // 心跳 PING/PONG

// 心跳状态码
constexpr int PING = 0;  // 服务器 → 客户端：询问存活
constexpr int PONG = 1;  // 客户端 → 服务器：确认存活

// ---------- 返回码（服务器 → 客户端） ----------

constexpr int OP_OK              = 0x80000000;  // 操作成功
constexpr int ONLINEUSER_OK      = 0x80000001;  // 在线用户列表项
constexpr int ONLINEUSER_OVER    = 0x80000002;  // 在线用户列表发送完毕
constexpr int NAME_EXIST         = 0x80000003;  // 注册失败：用户名已存在
constexpr int NAME_PWD_NMATCH    = 0x80000004;  // 登录失败：用户名或密码错误
constexpr int USER_LOGED         = 0x80000005;  // 登录失败：用户已在线
constexpr int USER_NOT_REGIST    = 0x80000006;  // 登录失败：用户未注册

// ---------- 网络 I/O 工具函数 ----------

//
// @brief  可靠读取指定字节数，处理 TCP 粘包/拆包。
//
// 循环调用 recv() 直到读满 len 字节或连接断开。
//
// @param  fd     socket 文件描述符。
// @param  buf    接收缓冲区。
// @param  len    期望读取的字节数。
// @return 实际读取的字节数，<= 0 表示错误或断开。
//
inline int recv_all(int fd, void* buf, int len) {
    int total = 0;
    while (total < len) {
        int n = recv(fd, static_cast<char*>(buf) + total, len - total, 0);
        if (n <= 0)
            return n;  // 错误或断开
        total += n;
    }
    return total;
}

//
// @brief  发送一条完整消息（头部 + 载荷）。
//
// 先发送固定大小的 Protocol 头部，再发送 data_len 字节的载荷。
//
// @param  fd       socket 文件描述符。
// @param  header   已填充的协议头部（data_len 必须已设置）。
// @param  data     载荷数据指针，data_len 为 0 时可为 nullptr。
//
inline void send_message(int fd, const Protocol& header, const void* data = nullptr) {
    // MSG_NOSIGNAL 避免向已关闭的 socket 写入时触发 SIGPIPE
    send(fd, &header, sizeof(header), MSG_NOSIGNAL);
    if (header.data_len > 0 && data != nullptr)
        send(fd, data, header.data_len, MSG_NOSIGNAL);
}

//
// @brief  接收一条完整消息（头部 + 载荷）。
//
// 先读取 Protocol 头部获取 data_len，再按长度读取载荷。
// 如果 data_len 超过 MAX_DATA_LEN，视为非法消息并返回空。
//
// @param  fd      socket 文件描述符。
// @param  header  [输出] 接收到的协议头部。
// @return 载荷数据的字符串（data_len 为 0 时返回空串）。
//
inline std::string recv_message(int fd, Protocol& header) {
    if (recv_all(fd, &header, sizeof(header)) <= 0)
        return {};

    // 安全检查：拒绝超大载荷
    if (header.data_len < 0 || header.data_len > MAX_DATA_LEN)
        return {};

    std::string data(header.data_len, '\0');
    if (header.data_len > 0)
        recv_all(fd, data.data(), header.data_len);
    return data;
}
