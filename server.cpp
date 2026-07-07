//
// server.cpp
// TCP 聊天室服务器 — 接受客户端连接，通过长度前缀分帧的 Protocol 消息
// 处理注册、登录、公聊、私聊、在线用户列表，维护在线用户状态。
// 使用读写锁保护全局用户表。
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
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

// ---------- 全局用户表 ----------

// 所有用户的注册信息和在线状态，大小固定为 MAX_USER_NUM
std::array<OnlineUser, MAX_USER_NUM> online{};

// 读写锁：保护 online[] 的多线程并发访问
// - shared_lock 用于纯读操作（handle_broadcast, broadcast_system_msg 等）
// - unique_lock 用于写操作（handle_register, find_user_online, del_user_online）
std::shared_mutex online_mutex;

// ---------- 用户查找辅助函数 ----------

//
// @brief  按用户名查找在线用户。
//
// 要求目标用户必须在线（fd != -1），用于私聊时查找消息接收方。
// 调用者已持有 online_mutex 的 shared_lock 或 unique_lock。
//
// @param  name  要搜索的用户名。
// @return 在 online[] 中的下标，未找到返回 -1。
//
int find_user_by_name(const char* name) {
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        if (online[i].flag != 1 || online[i].fd == -1)
            continue;
        if (std::strcmp(name, online[i].name) == 0)
            return i;
    }
    return -1;
}

// 前置声明
void del_user_online(int index);
void broadcast_system_msg(const char* text);

// ---------- 系统通知广播 ----------

//
// @brief  向所有在线用户广播一条系统消息。
//
// 持共享锁拷贝 fd 列表，解锁后发送（锁内不做 I/O）。
//
// @param  text  要广播的文本。
//
void broadcast_system_msg(const char* text) {
    // 持锁拷贝在线 fd 列表
    std::vector<int> fds;
    {
        std::shared_lock lock(online_mutex);
        for (int i = 0; i < MAX_USER_NUM; ++i) {
            if (online[i].fd != -1)
                fds.push_back(online[i].fd);
        }
    }

    // 锁外发送
    Protocol hdr{};
    hdr.data_len = std::strlen(text);
    for (int fd : fds)
        send_message(fd, hdr, text);
}

// ---------- 命令处理函数 ----------

//
// @brief  处理注册命令（REGISTE）。
//
// 持独占锁完成 find + add 操作，解锁后发送响应（锁内不做 I/O）。
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

    {
        std::unique_lock lock(online_mutex);

        // 查找用户名是否已被注册
        int dest = -1;
        for (int i = 0; i < MAX_USER_NUM; ++i) {
            if (online[i].flag == 1 && std::strcmp(hdr.name, online[i].name) == 0) {
                dest = i;
                break;
            }
        }

        if (dest == -1) {
            // 用户名可用 — 添加到用户表
            int slot = -1;
            for (int i = 0; i < MAX_USER_NUM; ++i) {
                if (online[i].flag == -1) {
                    online[i].flag = 1;
                    std::strncpy(online[i].name, hdr.name, sizeof(online[i].name) - 1);
                    std::strncpy(online[i].passwd, data.c_str(),
                                 sizeof(online[i].passwd) - 1);
                    slot = i;
                    break;
                }
            }
            *index = slot;
            resp.state = (slot >= 0) ? OP_OK : NAME_EXIST;
            std::cout << "用户 " << hdr.name << " 注册" << (slot >= 0 ? "成功" : "失败（表满）") << '\n';
        } else {
            resp.state = NAME_EXIST;
            std::cout << "用户 " << hdr.name << " 已存在\n";
        }
    }  // 解锁

    if (!send_message(sockfd, resp))
        perror("发送注册响应失败");
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
    bool success = false;

    {
        std::unique_lock lock(online_mutex);

        int ret = NAME_PWD_NMATCH;
        for (int i = 0; i < MAX_USER_NUM; ++i) {
            if (online[i].flag != 1)
                continue;
            if (std::strcmp(hdr.name, online[i].name) == 0 &&
                std::strcmp(data.c_str(), online[i].passwd) == 0) {
                if (online[i].fd == -1) {
                    online[i].fd = sockfd;
                    online[i].last_active = time(nullptr);
                    *index = i;
                    ret = OP_OK;
                } else {
                    std::cout << online[i].name << " 已在线，拒绝重复登录\n";
                    ret = USER_LOGED;
                }
                break;
            }
        }
        resp.state = ret;

        if (ret == OP_OK) {
            success = true;
            std::cout << "用户 " << hdr.name << " 登录成功（槽位 " << *index << "）\n";
        } else {
            std::cout << "用户 " << hdr.name << " 登录失败（错误码 " << ret << "）\n";
        }
    }  // 解锁

    if (success) {
        const char* msg = "login success\n";
        resp.data_len = std::strlen(msg);
        // 先发送登录成功响应（锁外）
        if (!send_message(sockfd, resp, msg))
            perror("发送登录响应失败");

        // 通知所有在线用户（锁外）
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s 上线", hdr.name);
        broadcast_system_msg(buf);
    } else {
        if (!send_message(sockfd, resp))
            perror("发送登录失败响应");
    }
}

//
// @brief  处理公聊命令（BROADCAST）。
//
// 将消息格式化为 "发送者: 内容"，然后发送给所有在线用户（排除发送者本人）。
// 持共享锁拷贝 fd 列表，解锁后发送（锁内不做 I/O）。
//
// @param  sender_index  发送者在 online[] 中的下标。
// @param  hdr           请求头部。
// @param  data          载荷（消息内容）。
//
void handle_broadcast(int sender_index, const Protocol& hdr,
                      const std::string& data) {
    // 格式化消息
    char buf[MAX_DATA_LEN];
    int len;
    {
        std::shared_lock lock(online_mutex);
        len = std::snprintf(buf, sizeof(buf), "%s: %s",
                            online[sender_index].name, data.c_str());
    }

    Protocol out{};
    out.data_len = len;
    std::cout << "广播 [" << buf << "]\n";

    // 持锁拷贝在线用户 fd 列表（排除发送者）
    std::vector<std::pair<int, int>> targets;  // (fd, index)
    {
        std::shared_lock lock(online_mutex);
        for (int i = 0; i < MAX_USER_NUM; ++i) {
            if (online[i].fd == -1 || i == sender_index)
                continue;
            targets.push_back({online[i].fd, i});
        }
    }

    // 锁外发送，失败时清理
    for (auto& [fd, idx] : targets) {
        if (!send_message(fd, out, buf)) {
            close(fd);
            del_user_online(idx);
        }
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
    // 持锁查找目标 + 获取发送者信息
    int target = -1;
    int sender_fd = -1;
    char sender_name[32] = {};
    char target_name[32] = {};
    {
        std::shared_lock lock(online_mutex);
        target = find_user_by_name(hdr.name);
        sender_fd = online[sender_index].fd;
        std::strncpy(sender_name, online[sender_index].name, sizeof(sender_name) - 1);
        if (target >= 0)
            std::strncpy(target_name, online[target].name, sizeof(target_name) - 1);
    }

    if (target == -1) {
        // 目标不存在或不在线 — 通知发送者
        char buf[128];
        int len = std::snprintf(buf, sizeof(buf), "用户 %s 不存在或不在线", hdr.name);
        Protocol resp{};
        resp.data_len = len;
        if (!send_message(sender_fd, resp, buf))
            perror("发送私聊错误通知失败");
        std::cout << "私聊失败：" << sender_name
                  << " → " << hdr.name << "（目标不在线）\n";
        return;
    }

    // 格式化并投递到目标用户（锁外）
    char buf[MAX_DATA_LEN];
    int len = std::snprintf(buf, sizeof(buf), "%s say to %s: %s",
                            sender_name, target_name, data.c_str());
    Protocol out{};
    out.data_len = len;

    // 持锁获取目标 fd
    int target_fd = -1;
    {
        std::shared_lock lock(online_mutex);
        if (target < MAX_USER_NUM)
            target_fd = online[target].fd;
    }

    if (target_fd >= 0 && !send_message(target_fd, out, buf))
        perror("发送私聊失败");
    std::cout << "私聊 [" << buf << "]\n";
}

//
// @brief  处理在线用户列表命令（ONLINEUSER）。
//
// 持共享锁遍历 + 按需拷贝用户名和 fd，解锁后发送。
//
// @param  requester_index  请求者在 online[] 中的下标。
//
void handle_online_users(int requester_index) {
    // 持锁获取请求方 fd 和在线用户名列表
    int requester_fd = -1;
    std::vector<std::string> names;
    {
        std::shared_lock lock(online_mutex);
        requester_fd = online[requester_index].fd;
        for (int i = 0; i < MAX_USER_NUM; ++i) {
            if (online[i].fd != -1)
                names.push_back(online[i].name);
        }
    }

    // 锁外发送
    Protocol out{};
    out.cmd = ONLINEUSER;

    for (const auto& name : names) {
        out.state = ONLINEUSER_OK;
        std::strncpy(out.name, name.c_str(), sizeof(out.name) - 1);
        if (!send_message(requester_fd, out)) {
            perror("发送在线列表失败");
            return;
        }
    }

    // 发送列表结束标记
    out.state = ONLINEUSER_OVER;
    out.name[0] = '\0';
    if (!send_message(requester_fd, out))
        perror("发送在线列表结束标记失败");

    // 持锁获取日志用名称
    {
        std::shared_lock lock(online_mutex);
        std::cout << "在线用户列表已发送给 " << online[requester_index].name << '\n';
    }
}

//
// @brief  将用户标记为离线，并通知所有在线用户。
//
// 持独占锁标记离线，解锁后发送通知。
//
// @param  index  在 online[] 中的下标。
//
void del_user_online(int index) {
    if (index < 0 || index >= MAX_USER_NUM)
        return;

    // 持锁标记离线 + 构建通知文本
    char buf[64];
    {
        std::unique_lock lock(online_mutex);
        std::cout << online[index].name << " 离线\n";
        std::snprintf(buf, sizeof(buf), "%s 离线", online[index].name);
        online[index].fd = -1;
    }

    // 持锁拷贝 fd 列表
    std::vector<int> fds;
    {
        std::shared_lock lock(online_mutex);
        for (int i = 0; i < MAX_USER_NUM; ++i) {
            if (online[i].fd != -1)
                fds.push_back(online[i].fd);
        }
    }

    // 锁外发送离线通知
    Protocol hdr{};
    hdr.data_len = std::strlen(buf);
    for (int fd : fds)
        send_message(fd, hdr, buf);
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

        // 已登录用户：任何收到的数据包都刷新活跃时间（atomic，无需锁）
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
            break;
        default:
            break;
        }
    }

    // 线程退出前清理
    del_user_online(index);
    close(client_fd);
}

// ---------- 心跳 watchdog 线程 ----------

//
// @brief  watchdog 线程，定期检测死连接。
//
// 持独占锁遍历收集待处理动作，解锁后执行（锁内不做 I/O）。
//
void watchdog_loop() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(WATCHDOG_TICK));

        time_t now = time(nullptr);

        // 持锁遍历，收集需要处理的用户
        std::vector<int> timeout_users;  // 需要强制下线的用户 index
        std::vector<int> ping_users;     // 需要发 PING 的用户 fd

        {
            std::unique_lock lock(online_mutex);
            for (int i = 0; i < MAX_USER_NUM; ++i) {
                if (online[i].fd == -1)
                    continue;

                time_t idle = now - online[i].last_active.load();

                if (idle >= TIMEOUT) {
                    timeout_users.push_back(i);
                } else if (idle >= PING_INTERVAL) {
                    ping_users.push_back(online[i].fd);
                }
            }
        }  // 解锁

        // 锁外执行：发 PING
        Protocol ping{};
        ping.cmd = HEARTBEAT;
        ping.state = PING;
        for (int fd : ping_users)
            send_message(fd, ping);

        // 锁外执行：强制下线（del_user_online 自身会加锁）
        for (int idx : timeout_users) {
            std::cout << "心跳超时: " << online[idx].name << " 已强制下线\n";
            close(online[idx].fd);
            del_user_online(idx);
        }
    }
}

// ---------- 信号处理（优雅关闭） ----------

namespace {

volatile sig_atomic_t g_shutdown = 0;
int                    g_listen_fd = -1;

void handle_signal(int /*sig*/) {
    g_shutdown = 1;
    if (g_listen_fd >= 0)
        close(g_listen_fd);
}

}  // namespace

// ---------- 入口 ----------

//
// @brief  聊天室服务器入口。
// @param  argc  参数个数（预期为 2）。
// @param  argv  参数数组 — argv[1] 为监听端口号。
// @return 0 正常退出，1 出错。
//
int main(int argc, char* argv[]) {
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

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listen_sock.get(), reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(listen_sock.get(), 10) == -1) {
        perror("listen");
        return 1;
    }

    std::cout << "服务器正在监听端口 " << port << '\n';

    g_listen_fd = listen_sock.get();

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    std::thread(watchdog_loop).detach();

    // 主循环
    while (!g_shutdown) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_fd =
            accept(listen_sock.get(), reinterpret_cast<sockaddr*>(&client_addr),
                   &addr_len);
        if (client_fd == -1) {
            if (g_shutdown)
                break;
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "客户端已连接: " << client_ip << '\n';

        std::thread(client_handler, client_fd, std::string(client_ip)).detach();
    }

    // 优雅关闭
    std::cout << "\n正在关闭服务器...\n";

    const char* shutdown_msg = "服务器正在关闭";
    Protocol hdr{};
    hdr.data_len = std::strlen(shutdown_msg);
    {
        std::unique_lock lock(online_mutex);
        for (int i = 0; i < MAX_USER_NUM; ++i) {
            if (online[i].fd != -1) {
                send_message(online[i].fd, hdr, shutdown_msg);
                close(online[i].fd);
                online[i].fd = -1;
            }
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "服务器已关闭，再见。\n";
    return 0;
}
