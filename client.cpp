//
// client.cpp
// TCP 聊天室客户端 — 连接到聊天室服务器，提供交互式菜单，
// 支持注册、登录、公聊、私聊、在线用户列表。
// 使用长度前缀分帧协议通信。
//
// 用法：
//   compile:  cmake -B build && cmake --build build
//   run:      ./build/client <host> <port>
//   example:  ./build/client 127.0.0.1 8888
//
// 菜单流程：
//   未登录 → [1.注册] [2.登录] [0.退出]
//   已登录 → [3.公聊] [4.私聊] [5.在线用户] [0.登出]
//

#include "socket.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

// ---------- 全局连接状态 ----------

namespace {

int  sockfd  = -1;        // 与服务器的连接 socket
int  login_f = -1;        // -1：未登录，1：已登录

}  // namespace

// ---------- 后台接收线程 ----------

//
// @brief  后台线程，负责接收服务器发来的消息。
//
// 登录后持续运行。根据 cmd 和 state 区分消息类型：
// - ONLINEUSER_OK：同行打印用户名（tab 分隔）
// - ONLINEUSER_OVER：打印换行，列表结束
// - 其他消息：打印载荷内容
//
void recv_loop() {
    while (true) {
        // 等待用户登录后才开始读取 socket。
        if (login_f != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // 先读取固定大小头部
        Protocol hdr{};
        if (recv_all(sockfd, &hdr, sizeof(hdr)) <= 0) {
            std::cout << "\n[与服务器断开连接]\n";
            login_f = -1;
            return;
        }

        // 安全检查
        if (hdr.data_len < 0 || hdr.data_len > MAX_DATA_LEN)
            continue;

        // 按声明的长度读取载荷
        std::string data(hdr.data_len, '\0');
        if (hdr.data_len > 0) {
            if (recv_all(sockfd, data.data(), hdr.data_len) <= 0) {
                std::cout << "\n[与服务器断开连接]\n";
                login_f = -1;
                return;
            }
        }

        // 心跳 PING — 自动回复 PONG，对用户透明
        if (hdr.cmd == HEARTBEAT && hdr.state == PING) {
            Protocol pong{};
            pong.cmd = HEARTBEAT;
            pong.state = PONG;
            if (!send_message(sockfd, pong)) {
                std::cout << "\n[与服务器断开连接]\n";
                login_f = -1;
                return;
            }
            continue;
        }

        // 在线用户列表响应 — 特殊显示格式
        if (hdr.cmd == ONLINEUSER) {
            if (hdr.state == ONLINEUSER_OK) {
                // 每个用户名同行显示，用 tab 分隔
                std::cout << hdr.name << "\t";
            } else if (hdr.state == ONLINEUSER_OVER) {
                // 列表结束，换行
                std::cout << "\n";
            }
            continue;
        }

        // 普通聊天消息、系统通知 — 直接打印载荷内容
        if (!data.empty())
            std::cout << data << '\n';
    }
}

// ---------- 用户操作 ----------

//
// @brief  注册新账号。
//
// 提示输入用户名和密码，密码作为载荷发送。
//
void do_register() {
    Protocol hdr{};
    hdr.cmd = REGISTE;

    std::string password;
    std::cout << "用户名: ";
    std::cin >> hdr.name;
    std::cout << "密码: ";
    std::cin >> password;

    hdr.data_len = password.size();
    if (!send_message(sockfd, hdr, password.data())) {
        std::cout << "[发送失败，请检查连接]\n";
        return;
    }

    // 等待服务器返回注册结果（仅头部，无载荷）
    Protocol resp{};
    if (recv_all(sockfd, &resp, sizeof(resp)) <= 0)
        return;

    if (resp.state == OP_OK)
        std::cout << "[注册成功]\n";
    else if (resp.state == NAME_EXIST)
        std::cout << "[用户名已存在]\n";

    std::cin.ignore(1024, '\n');
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

//
// @brief  使用已有账号登录。
//
// 提示输入用户名和密码，密码作为载荷发送。
//
void do_login() {
    Protocol hdr{};
    hdr.cmd = LOGIN;

    std::string password;
    std::cout << "用户名: ";
    std::cin >> hdr.name;
    std::cout << "密码: ";
    std::cin >> password;

    hdr.data_len = password.size();
    if (!send_message(sockfd, hdr, password.data())) {
        std::cout << "[发送失败，请检查连接]\n";
        return;
    }

    // 等待服务器返回登录结果
    Protocol resp{};
    if (recv_all(sockfd, &resp, sizeof(resp)) <= 0)
        return;

    // 如果响应带有载荷（如 "login success\n"），读取后丢弃（仅日志用）
    std::string data(resp.data_len, '\0');
    if (resp.data_len > 0 && resp.data_len <= MAX_DATA_LEN)
        recv_all(sockfd, data.data(), resp.data_len);

    // 根据返回码处理不同情况
    switch (resp.state) {
    case OP_OK:
        std::cout << "[登录成功]\n";
        login_f = 1;          // 切换为已登录状态，后续菜单展示聊天选项
        break;
    case NAME_PWD_NMATCH:
        std::cout << "[用户名或密码错误]\n";
        break;
    case USER_LOGED:
        std::cout << "[该用户已在线]\n";
        break;
    default:
        std::cout << "[登录失败，错误码: " << resp.state << "]\n";
        break;
    }

    std::cin.ignore(1024, '\n');
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

//
// @brief  发送公聊消息。
//
// 消息内容作为载荷发送（支持空格、超长文本）。
//
void do_broadcast() {
    Protocol hdr{};
    hdr.cmd = BROADCAST;

    std::cout << "消息: ";
    std::cin.ignore(1024, '\n');
    std::string msg;
    std::getline(std::cin, msg);

    hdr.data_len = msg.size();
    if (!send_message(sockfd, hdr, msg.data()))
        std::cout << "[发送失败]\n";
}

//
// @brief  发送私聊消息。
//
// 目标用户名放在头部 name 字段，消息内容作为载荷发送。
//
void do_private() {
    Protocol hdr{};
    hdr.cmd = PRIVATE;

    std::cout << "发送给: ";
    std::cin.ignore(1024, '\n');
    std::cin.getline(hdr.name, sizeof(hdr.name) - 1);

    std::cout << "消息: ";
    std::string msg;
    std::getline(std::cin, msg);

    hdr.data_len = msg.size();
    if (!send_message(sockfd, hdr, msg.data()))
        std::cout << "[发送失败]\n";
}

//
// @brief  请求在线用户列表。
//
// 发送 ONLINEUSER 命令（无载荷），接收由后台 recv_loop 处理。
//
void do_list_online() {
    Protocol hdr{};
    hdr.cmd = ONLINEUSER;

    if (!send_message(sockfd, hdr))
        std::cout << "[发送失败]\n";
    // 等待 recv_loop 接收并打印列表
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

//
// @brief  登出并返回登录/注册菜单。
//
void do_logout() {
    close(sockfd);
    sockfd = -1;
    login_f = -1;
}

// ---------- 菜单显示 ----------

//
// @brief  显示未登录状态的菜单。
//
void show_login_menu() {
    std::cout << "\n========== 聊天室 ==========\n";
    std::cout << "  1. 注册\n";
    std::cout << "  2. 登录\n";
    std::cout << "  0. 退出\n";
    std::cout << "============================\n";
    std::cout << "选择: ";
}

//
// @brief  显示已登录状态的菜单。
//
void show_chat_menu() {
    std::cout << "\n========== 聊天室 ==========\n";
    std::cout << "  3. 公聊\n";
    std::cout << "  4. 私聊\n";
    std::cout << "  5. 在线用户\n";
    std::cout << "  0. 登出\n";
    std::cout << "============================\n";
    std::cout << "选择: ";
}

// ---------- 入口 ----------

//
// @brief  聊天室客户端入口。
// @param  argc  参数个数（预期为 3）。
// @param  argv  参数数组 — argv[1] 服务器 IP，argv[2] 端口号。
// @return 0 正常退出，1 出错。
//
int main(int argc, char* argv[]) {
    // 参数校验
    if (argc != 3) {
        std::cerr << "用法: " << argv[0] << " host port\n";
        return 1;
    }

    int port = std::atoi(argv[2]);
    if (port <= 0) {
        std::cerr << "用法: " << argv[0] << " host port\n";
        return 1;
    }

    // 创建 socket 并连接服务器
    ScopedSocket sock(socket(AF_INET, SOCK_STREAM, 0));
    if (sock.get() == -1) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        std::cerr << "无效地址: " << argv[1] << '\n';
        return 1;
    }

    if (connect(sock.get(), reinterpret_cast<sockaddr*>(&server_addr),
                sizeof(server_addr)) == -1) {
        perror("connect");
        return 1;
    }

    sockfd = sock.release();  // 转移所有权给全局变量，ScopedSocket 不再管理
    std::cout << "已连接到 " << argv[1] << ':' << port << '\n';

    // 启动后台接收线程（登录后生效）
    std::thread recv_thread(recv_loop);
    recv_thread.detach();

    // 主菜单循环
    while (true) {
        // 根据登录状态显示不同的菜单
        if (login_f == -1)
            show_login_menu();
        else
            show_chat_menu();

        int sel;
        std::cin >> sel;

        if (!std::cin) {
            // 收到 EOF（Ctrl+D），正常退出
            if (std::cin.eof())
                break;
            // 非数字输入 — 清空并重试
            std::cin.clear();
            std::cin.ignore(1024, '\n');
            continue;
        }

        if (sel == 0) {
            if (login_f == 1) {
                // 已登录状态：0 表示登出
                do_logout();
            } else {
                // 未登录状态：0 表示退出程序
                break;
            }
            continue;
        }

        // 未登录时仅允许 1（注册）和 2（登录）
        if (login_f == -1) {
            if (sel < 1 || sel > 2) {
                std::cout << "[无效选项]\n";
                continue;
            }
            if (sel == 1)
                do_register();
            else
                do_login();
        } else {
            // 已登录时仅允许 3（公聊）、4（私聊）、5（在线用户）
            if (sel < 3 || sel > 5) {
                std::cout << "[无效选项]\n";
                continue;
            }
            switch (sel) {
            case 3:
                do_broadcast();
                break;
            case 4:
                do_private();
                break;
            case 5:
                do_list_online();
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
