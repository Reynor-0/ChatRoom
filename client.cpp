//
// client.cpp
// TCP Chatroom Client — connects to the chatroom server, provides an
// interactive menu for registration, login, and (in future phases)
// chat functionality.
//
// Usage:
//   compile:  cmake -B build && cmake --build build
//   run:      ./build/client <host> <port>
//   example:  ./build/client 127.0.0.1 8888
//
// Menu flow:
//   Not logged in → [1.Register] [2.Login] [0.Exit]
//   Logged in     → [3.Broadcast] [4.Private] [5.Online Users] [0.Logout]
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

// ---------- Global connection state ----------

namespace {

int  sockfd  = -1;        // server connection
int  login_f = -1;        // -1 = not logged in, 1 = logged in

}  // namespace

// ---------- Background receive thread ----------

//
// @brief  Background thread that receives server messages.
//
// Runs for the lifetime of the client. While not logged in it sleeps
// briefly; once logged in it reads Protocol structs and prints
// the data field (used for broadcasts, system messages, etc.).
//
void recv_loop() {
    while (true) {
        // Wait until the user is logged in before reading from the socket.
        // During register/login the main thread handles socket I/O directly.
        if (login_f != 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        Protocol msg{};
        int n = recv(sockfd, &msg, sizeof(msg), 0);
        if (n <= 0) {
            std::cout << "\n[Disconnected from server]\n";
            login_f = -1;
            return;
        }

        if (static_cast<size_t>(n) < sizeof(msg))
            continue;

        // Print the server message (data field carries text for now)
        if (msg.data[0] != '\0')
            std::cout << msg.data << '\n';
    }
}

// ---------- User actions ----------

//
// @brief  Register a new account.
//
// Prompts for username and password, sends a REGISTE command,
// and prints the server's response.
//
void do_register() {
    Protocol msg{};
    msg.cmd = REGISTE;

    std::cout << "Username: ";
    std::cin >> msg.name;
    std::cout << "Password: ";
    std::cin >> msg.data;

    send(sockfd, &msg, sizeof(msg), 0);

    Protocol resp{};
    recv(sockfd, &resp, sizeof(resp), 0);

    if (resp.state == OP_OK)
        std::cout << "[Registration successful]\n";
    else if (resp.state == NAME_EXIST)
        std::cout << "[Username already exists]\n";

    std::cin.ignore(1024, '\n');
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

//
// @brief  Login with existing credentials.
//
// Prompts for username and password, sends a LOGIN command,
// and updates the global login_f state on success.
//
void do_login() {
    Protocol msg{};
    msg.cmd = LOGIN;

    std::cout << "Username: ";
    std::cin >> msg.name;
    std::cout << "Password: ";
    std::cin >> msg.data;

    send(sockfd, &msg, sizeof(msg), 0);

    Protocol resp{};
    recv(sockfd, &resp, sizeof(resp), 0);

    switch (resp.state) {
    case OP_OK:
        std::cout << "[Login successful]\n";
        login_f = 1;
        break;
    case NAME_PWD_NMATCH:
        std::cout << "[Wrong username or password]\n";
        break;
    case USER_LOGED:
        std::cout << "[User already logged in]\n";
        break;
    default:
        std::cout << "[Login failed, code: " << resp.state << "]\n";
        break;
    }

    std::cin.ignore(1024, '\n');
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

//
// @brief  Logout and return to the login/register menu.
//
void do_logout() {
    // Close the socket to signal server
    close(sockfd);
    sockfd = -1;
    login_f = -1;
}

// ---------- Menu display ----------

//
// @brief  Display the menu for not-logged-in users.
//
void show_login_menu() {
    std::cout << "\n========== Chatroom ==========\n";
    std::cout << "  1. Register\n";
    std::cout << "  2. Login\n";
    std::cout << "  0. Exit\n";
    std::cout << "==============================\n";
    std::cout << "Choice: ";
}

//
// @brief  Display the menu for logged-in users.
//
void show_chat_menu() {
    std::cout << "\n========== Chatroom ==========\n";
    std::cout << "  3. Broadcast\n";
    std::cout << "  4. Private message\n";
    std::cout << "  5. Online users\n";
    std::cout << "  0. Logout\n";
    std::cout << "==============================\n";
    std::cout << "Choice: ";
}

// ---------- Entry point ----------

//
// @brief  Entry point for the chatroom client.
// @param  argc  Argument count (expected: 3).
// @param  argv  Argument vector — argv[1] is server IP, argv[2] is port.
// @return 0 on clean exit, 1 on error.
//
int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " host port\n";
        return 1;
    }

    int port = std::atoi(argv[2]);
    if (port <= 0) {
        std::cerr << "Usage: " << argv[0] << " host port\n";
        return 1;
    }

    // Create socket and connect to server
    ScopedSocket sock(socket(AF_INET, SOCK_STREAM, 0));
    if (sock.get() == -1) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        std::cerr << "Invalid address: " << argv[1] << '\n';
        return 1;
    }

    if (connect(sock.get(), reinterpret_cast<sockaddr*>(&server_addr),
                sizeof(server_addr)) == -1) {
        perror("connect");
        return 1;
    }

    sockfd = sock.get();
    std::cout << "Connected to " << argv[1] << ':' << port << '\n';

    // Start the background receive thread (activates after login)
    std::thread recv_thread(recv_loop);
    recv_thread.detach();

    // Menu loop
    while (true) {
        if (login_f == -1)
            show_login_menu();
        else
            show_chat_menu();

        int sel;
        std::cin >> sel;

        if (!std::cin) {
            // EOF — clean exit
            if (std::cin.eof())
                break;
            // Non-numeric input — skip and retry
            std::cin.clear();
            std::cin.ignore(1024, '\n');
            continue;
        }

        if (sel == 0) {
            if (login_f == 1) {
                do_logout();
            } else {
                break;  // exit
            }
            continue;
        }

        if (login_f == -1) {
            // Not logged in: only 1 and 2 are valid
            if (sel < 1 || sel > 2) {
                std::cout << "[Invalid choice]\n";
                continue;
            }
            if (sel == 1)
                do_register();
            else
                do_login();
        } else {
            // Logged in: 3, 4, 5 are valid
            if (sel < 3 || sel > 5) {
                std::cout << "[Invalid choice]\n";
                continue;
            }
            // Phase 3+ will implement these
            switch (sel) {
            case 3:
                std::cout << "[Broadcast — coming in Phase 3]\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
            case 4:
                std::cout << "[Private message — coming in Phase 3]\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
            case 5:
                std::cout << "[Online users — coming in Phase 3]\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
