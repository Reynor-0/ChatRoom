//
// server.cpp
// TCP 聊天室服务器 — 接受客户端连接，通过长度前缀分帧的 Protocol 消息
// 处理注册、登录、公聊、私聊、在线用户列表，维护在线用户状态。
//
// 用法：
//   compile:  cmake -B build && cmake --build build
//   run:      ./build/server <port>
//   example:  ./build/server 8888
//

#include "socket.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

// ---------- 全局用户表 ----------

// 所有用户的注册信息和在线状态，大小固定为 MAX_USER_NUM
std::array<OnlineUser, MAX_USER_NUM> online{};

// ---------- 用户查找辅助函数 ----------

//
// @brief  按用户名查找已注册用户。
// @param  name  要搜索的用户名。
// @return 在 online[] 中的下标，未找到返回 -1。
//
int find_user(const char* name) {
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        // 仅检查已注册的槽位（flag == 1）
        if (online[i].flag == 1 && std::strcmp(name, online[i].name) == 0)
            return i;
    }
    return -1;
}

//
// @brief  按用户名查找在线用户。
//
// 与 find_user() 不同，本函数要求目标用户必须在线（fd != -1）。
// 用于私聊时查找消息接收方。
//
// @param  name  要搜索的用户名。
// @return 在 online[] 中的下标，未找到（不存在或不在线）返回 -1。
//
int find_user_by_name(const char* name) {
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        // 跳过空槽位、未注册条目和不在线的用户
        if (online[i].flag != 1 || online[i].fd == -1)
            continue;
        if (std::strcmp(name, online[i].name) == 0)
            return i;
    }
    return -1;
}

//
// @brief  验证登录凭据并标记用户上线。
//
// 遍历在线用户表，匹配用户名和密码。匹配成功后检查是否已在线：
// - 未在线：记录 socket fd，标记上线
// - 已在线：拒绝重复登录
//
// @param  sockfd   客户端的 socket fd。
// @param  index    [输出] 成功时设为该用户在 online[] 中的下标。
// @param  username 用户名。
// @param  password 密码。
// @return OP_OK 登录成功，USER_LOGED 已在线，NAME_PWD_NMATCH 凭据不匹配。
//
int find_user_online(int sockfd, int* index, const char* username,
                     const char* password) {
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        // 跳过空槽位和未注册的条目
        if (online[i].flag != 1)
            continue;

        // 比对用户名和密码
        if (std::strcmp(username, online[i].name) == 0 &&
            std::strcmp(password, online[i].passwd) == 0) {
            // 用户名密码匹配 — 检查是否已在线
            if (online[i].fd == -1) {
                online[i].fd = sockfd;   // 绑定当前 socket
                online[i].last_active = time(nullptr);  // 初始化活跃时间
                *index = i;
                return OP_OK;
            } else {
                std::cout << online[i].name << " 已在线，拒绝重复登录\n";
                return USER_LOGED;
            }
        }
    }
    // 遍历完所有条目仍未匹配
    return NAME_PWD_NMATCH;
}

//
// @brief  添加新用户注册条目。
//
// 在 online[] 中寻找第一个空槽位并填入注册信息。
//
// @param  username  用户名。
// @param  password  密码。
// @return 新条目的下标，表满时返回 -1。
//
int add_user(const char* username, const char* password) {
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        // 找到第一个空槽位（flag == -1）
        if (online[i].flag == -1) {
            online[i].flag = 1;
            std::strncpy(online[i].name, username, sizeof(online[i].name) - 1);
            std::strncpy(online[i].passwd, password, sizeof(online[i].passwd) - 1);
            std::cout << "注册 " << username << " 到槽位 " << i << '\n';
            return i;
        }
    }
    return -1;
}

// ---------- 系统通知广播 ----------

//
// @brief  向所有在线用户广播一条系统消息。
//
// 用于通知用户上线/离线等事件。
//
// @param  text  要广播的文本。
//
void broadcast_system_msg(const char* text) {
    Protocol hdr{};
    hdr.data_len = std::strlen(text);

    for (int i = 0; i < MAX_USER_NUM; ++i) {
        if (online[i].fd != -1)
            send_message(online[i].fd, hdr, text);
    }
}

// ---------- 命令处理函数 ----------

//
// @brief  处理注册命令（REGISTE）。
//
// 先检查用户名是否已存在：不存在则添加，存在则返回 NAME_EXIST。
//
// @param  sockfd  客户端 socket fd。
// @param  index   [输出] 成功时记录该用户在 online[] 中的下标。
// @param  hdr     请求头部（name=用户名）。
// @param  data    载荷（密码）。
//
void handle_register(int sockfd, int* index, const Protocol& hdr,
                     const std::string& data) {
    Protocol resp{};
    resp.cmd = REGISTE;

    // 查找用户名是否已被注册
    int dest = find_user(hdr.name);
    if (dest == -1) {
        // 用户名可用 — 添加到用户表
        *index = add_user(hdr.name, data.c_str());
        resp.state = OP_OK;
        std::cout << "用户 " << hdr.name << " 注册成功\n";
    } else {
        // 用户名已被占用
        resp.state = NAME_EXIST;
        std::cout << "用户 " << hdr.name << " 已存在\n";
    }

    send_message(sockfd, resp);
}

//
// @brief  处理登录命令（LOGIN）。
//
// 验证用户名和密码，成功则标记用户上线、返回 OP_OK，
// 并通知所有在线用户有新用户加入。
//
// @param  sockfd  客户端 socket fd。
// @param  index   [输出] 成功时记录该用户在 online[] 中的下标。
// @param  hdr     请求头部（name=用户名）。
// @param  data    载荷（密码）。
//
void handle_login(int sockfd, int* index, const Protocol& hdr,
                  const std::string& data) {
    Protocol resp{};
    resp.cmd = LOGIN;

    // 调用凭据验证函数
    int ret = find_user_online(sockfd, index, hdr.name, data.c_str());
    if (ret == OP_OK) {
        resp.state = OP_OK;
        const char* msg = "login success\n";
        resp.data_len = std::strlen(msg);
        std::cout << "用户 " << hdr.name << " 登录成功（槽位 " << *index << "）\n";

        // 先发送登录成功响应
        send_message(sockfd, resp, msg);

        // 通知所有在线用户：xxx 上线了
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s 上线", online[*index].name);
        broadcast_system_msg(buf);
    } else {
        // 将错误码直接透传给客户端
        resp.state = ret;
        std::cout << "用户 " << hdr.name << " 登录失败（错误码 " << ret << "）\n";
        send_message(sockfd, resp);
    }
}

//
// @brief  处理公聊命令（BROADCAST）。
//
// 将消息格式化为 "发送者: 内容"，然后发送给所有在线用户（排除发送者本人）。
//
// @param  sender_index  发送者在 online[] 中的下标。
// @param  hdr           请求头部。
// @param  data          载荷（消息内容）。
//
void handle_broadcast(int sender_index, const Protocol& hdr,
                      const std::string& data) {
    // 格式化消息：发送者名 + 消息内容
    char buf[MAX_DATA_LEN];
    int len = std::snprintf(buf, sizeof(buf), "%s: %s",
                            online[sender_index].name, data.c_str());

    Protocol out{};
    out.data_len = len;
    std::cout << "广播 [" << buf << "]\n";

    // 发送给所有在线用户，排除发送者本人
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        if (online[i].fd == -1 || i == sender_index)
            continue;
        send_message(online[i].fd, out, buf);
    }
}

//
// @brief  处理私聊命令（PRIVATE）。
//
// 查找目标用户，若在线则投递消息；
// 若目标不存在或不在线，则通知发送者。
//
// @param  sender_index  发送者在 online[] 中的下标。
// @param  hdr           请求头部（name=目标用户名）。
// @param  data          载荷（消息内容）。
//
void handle_private(int sender_index, const Protocol& hdr,
                    const std::string& data) {
    // 查找目标用户（必须在线）
    int target = find_user_by_name(hdr.name);
    if (target == -1) {
        // 目标不存在或不在线 — 通知发送者
        char buf[128];
        int len = std::snprintf(buf, sizeof(buf), "用户 %s 不存在或不在线", hdr.name);
        Protocol resp{};
        resp.data_len = len;
        send_message(online[sender_index].fd, resp, buf);
        std::cout << "私聊失败：" << online[sender_index].name
                  << " → " << hdr.name << "（目标不在线）\n";
        return;
    }

    // 格式化并投递到目标用户
    char buf[MAX_DATA_LEN];
    int len = std::snprintf(buf, sizeof(buf), "%s say to %s: %s",
                            online[sender_index].name, online[target].name,
                            data.c_str());
    Protocol out{};
    out.data_len = len;
    send_message(online[target].fd, out, buf);
    std::cout << "私聊 [" << buf << "]\n";
}

//
// @brief  处理在线用户列表命令（ONLINEUSER）。
//
// 遍历所有在线用户，逐个发送 ONLINEUSER_OK（含用户名），
// 最后发送 ONLINEUSER_OVER 标记列表结束。
//
// @param  requester_index  请求者在 online[] 中的下标。
//
void handle_online_users(int requester_index) {
    Protocol out{};
    out.cmd = ONLINEUSER;

    // 逐个发送在线用户名
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        if (online[i].fd == -1)
            continue;
        out.state = ONLINEUSER_OK;
        std::strncpy(out.name, online[i].name, sizeof(out.name) - 1);
        send_message(online[requester_index].fd, out);
    }

    // 发送列表结束标记
    out.state = ONLINEUSER_OVER;
    out.name[0] = '\0';
    send_message(online[requester_index].fd, out);

    std::cout << "在线用户列表已发送给 " << online[requester_index].name << '\n';
}

//
// @brief  将用户标记为离线，并通知所有在线用户。
//
// @param  index  在 online[] 中的下标。
//
void del_user_online(int index) {
    // 边界检查：防止越界访问
    if (index < 0 || index >= MAX_USER_NUM)
        return;

    std::cout << online[index].name << " 离线\n";

    // 先重置 fd（排除该用户），再广播离线通知
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s 离线", online[index].name);
    online[index].fd = -1;
    broadcast_system_msg(buf);
}

// ---------- 客户端处理线程 ----------

//
// @brief  单个客户端的处理线程函数。
//
// 循环接收 Protocol 消息（头部 + 载荷），根据 cmd 字段分发到对应处理函数。
// 运行至客户端断开连接或发生 recv 错误为止。
//
// @param  client_fd  已 accept 的客户端 socket fd。
// @param  client_ip  客户端 IP 字符串（用于日志）。
//
void client_handler(int client_fd, const std::string& client_ip) {
    Protocol hdr{};
    int index = -1;  // 该客户端在 online[] 中的槽位，-1 表示尚未登录

    while (true) {
        // 先收头部
        if (recv_all(client_fd, &hdr, sizeof(hdr)) <= 0) {
            if (errno != 0)
                perror("recv");
            else
                std::cout << "客户端断开: " << client_ip << '\n';
            break;
        }

        // 安全检查：拒绝超大载荷
        if (hdr.data_len < 0 || hdr.data_len > MAX_DATA_LEN)
            break;

        // 按头部中声明的长度收载荷
        std::string data(hdr.data_len, '\0');
        if (hdr.data_len > 0) {
            if (recv_all(client_fd, data.data(), hdr.data_len) <= 0)
                break;
        }

        // 已登录用户：任何收到的数据包都刷新活跃时间
        if (index >= 0)
            online[index].last_active = time(nullptr);

        // 根据命令码分派到对应的处理函数
        switch (hdr.cmd) {
        case REGISTE:
            handle_register(client_fd, &index, hdr, data);
            break;
        case LOGIN:
            handle_login(client_fd, &index, hdr, data);
            break;
        case BROADCAST:
            handle_broadcast(index, hdr, data);
            break;
        case PRIVATE:
            handle_private(index, hdr, data);
            break;
        case ONLINEUSER:
            handle_online_users(index);
            break;
        case HEARTBEAT:
            // PONG 已通过上方的 last_active 刷新处理，
            // 客户端发来的 PING 直接忽略即可
            break;
        default:
            break;
        }
    }

    // 线程退出前清理：如果该客户端已登录，标记为离线并通知其他人
    del_user_online(index);
    close(client_fd);
}

// ---------- 心跳 watchdog 线程 ----------

//
// @brief  watchdog 线程，定期检测死连接。
//
// 每 WATCHDOG_TICK 秒检查所有在线用户：
// - 空闲 > PING_INTERVAL：发送 PING 探测
// - 空闲 > TIMEOUT：判定为死连接，强制关闭并通知其他用户
//
void watchdog_loop() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(WATCHDOG_TICK));

        time_t now = time(nullptr);

        for (int i = 0; i < MAX_USER_NUM; ++i) {
            if (online[i].fd == -1)
                continue;

            time_t idle = now - online[i].last_active.load();

            if (idle >= TIMEOUT) {
                // 超时未响应 — 强制断开
                std::cout << "心跳超时: " << online[i].name << " 已强制下线\n";
                close(online[i].fd);
                del_user_online(i);
            } else if (idle >= PING_INTERVAL) {
                // 空闲超过 PING 间隔 — 发送 PING
                Protocol ping{};
                ping.cmd = HEARTBEAT;
                ping.state = PING;
                send_message(online[i].fd, ping);
            }
        }
    }
}

// ---------- 入口 ----------

//
// @brief  聊天室服务器入口。
// @param  argc  参数个数（预期为 2）。
// @param  argv  参数数组 — argv[1] 为监听端口号。
// @return 0 正常退出，1 出错。
//
int main(int argc, char* argv[]) {
    // 参数校验
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " port\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0) {
        std::cerr << "用法: " << argv[0] << " port\n";
        return 1;
    }

    // 初始化用户表 — 所有槽位设为空
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        online[i].fd = -1;
        online[i].flag = -1;
        online[i].last_active = time(nullptr);
    }

    // 创建监听 socket
    ScopedSocket listen_sock(socket(AF_INET, SOCK_STREAM, 0));
    if (listen_sock.get() == -1) {
        perror("socket");
        return 1;
    }

    // 绑定到所有网卡接口的指定端口
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listen_sock.get(), reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) == -1) {
        perror("bind");
        return 1;
    }

    // 开始监听（backlog = 10）
    if (listen(listen_sock.get(), 10) == -1) {
        perror("listen");
        return 1;
    }

    std::cout << "服务器正在监听端口 " << port << '\n';

    // 启动心跳 watchdog 线程
    std::thread(watchdog_loop).detach();

    // 主循环：阻塞等待客户端连接
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_fd =
            accept(listen_sock.get(), reinterpret_cast<sockaddr*>(&client_addr),
                   &addr_len);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        // 解析客户端 IP 用于日志
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "客户端已连接: " << client_ip << '\n';

        // 每个客户端在独立线程中处理（detach 方式）
        std::thread(client_handler, client_fd, std::string(client_ip)).detach();
    }
}
