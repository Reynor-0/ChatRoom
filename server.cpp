//
// server.cpp
// TCP Chatroom Server — accepts client connections, handles registration
// and login via structured Protocol messages, and maintains an online
// user list for future broadcast / private messaging features.
//
// Usage:
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

// ---------- Global user table ----------

std::array<OnlineUser, MAX_USER_NUM> online{};

// ---------- User lookup helpers ----------

//
// @brief  Find a registered user by name.
// @param  name  Username to search for.
// @return Index into `online[]`, or -1 if not found.
//
int find_user(const char* name) {
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        if (online[i].flag == 1 && std::strcmp(name, online[i].name) == 0)
            return i;
    }
    return -1;
}

//
// @brief  Verify login credentials and mark user online.
//
// Searches for a matching (name, passwd) pair. If found and the user is
// not already online, records the socket fd in the user's record.
//
// @param  sockfd  Client socket fd.
// @param  index   [out] Set to the user's index in `online[]` on success.
// @param  msg     Incoming LOGIN request containing name and data (password).
// @return OP_OK, USER_LOGED, or NAME_PWD_NMATCH.
//
int find_user_online(int sockfd, int* index, const Protocol* msg) {
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        if (online[i].flag != 1)
            continue;

        if (std::strcmp(msg->name, online[i].name) == 0 &&
            std::strcmp(msg->data, online[i].passwd) == 0) {
            if (online[i].fd == -1) {
                online[i].fd = sockfd;
                *index = i;
                return OP_OK;
            } else {
                std::cout << online[i].name << " already logged in\n";
                return USER_LOGED;
            }
        }
    }
    return NAME_PWD_NMATCH;
}

//
// @brief  Add a new user registration entry.
// @param  msg  REGISTE request containing name and data (password).
// @return Index of the new entry, or -1 if the table is full.
//
int add_user(const Protocol* msg) {
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        if (online[i].flag == -1) {
            online[i].flag = 1;
            std::strncpy(online[i].name, msg->name, sizeof(online[i].name) - 1);
            std::strncpy(online[i].passwd, msg->data, sizeof(online[i].passwd) - 1);
            std::cout << "Registered " << msg->name << " at slot " << i << '\n';
            return i;
        }
    }
    return -1;
}

// ---------- Command handlers ----------

//
// @brief  Handle a REGISTE command.
//
// Checks if the username already exists. If not, adds a new entry.
//
// @param  sockfd  Client socket fd.
// @param  index   [out] Set to the user's index on success.
// @param  msg     Incoming REGISTE request.
//
void handle_register(int sockfd, int* index, const Protocol* msg) {
    Protocol resp{};
    resp.cmd = REGISTE;

    int dest = find_user(msg->name);
    if (dest == -1) {
        // Username available — add to the table
        *index = add_user(msg);
        resp.state = OP_OK;
        std::cout << "User " << msg->name << " registered successfully\n";
    } else {
        resp.state = NAME_EXIST;
        std::cout << "User " << msg->name << " already exists\n";
    }

    send(sockfd, &resp, sizeof(resp), 0);
}

//
// @brief  Handle a LOGIN command.
//
// Verifies credentials and marks the user as online.
//
// @param  sockfd  Client socket fd.
// @param  index   [out] Set to the user's index on success.
// @param  msg     Incoming LOGIN request.
//
void handle_login(int sockfd, int* index, const Protocol* msg) {
    Protocol resp{};
    resp.cmd = LOGIN;

    int ret = find_user_online(sockfd, index, msg);
    if (ret == OP_OK) {
        resp.state = OP_OK;
        std::strncpy(resp.data, "login success\n", sizeof(resp.data) - 1);
        std::cout << "User " << msg->name << " logged in (slot " << *index << ")\n";
    } else {
        resp.state = ret;
        std::cout << "User " << msg->name << " login failed (code " << ret << ")\n";
    }

    send(sockfd, &resp, sizeof(resp), 0);
}

//
// @brief  Mark a user as offline.
// @param  index  Index in the online user table.
//
void del_user_online(int index) {
    if (index < 0 || index >= MAX_USER_NUM)
        return;

    std::cout << online[index].name << " went offline\n";
    online[index].fd = -1;
}

// ---------- Client handler thread ----------

//
// @brief  Per-client thread function.
//
// Receives Protocol messages and dispatches them to the appropriate handler.
// Runs until the client disconnects or an error occurs.
//
// @param  client_fd   The accepted client socket fd.
// @param  client_ip   IP string of the client (for logging).
//
void client_handler(int client_fd, const std::string& client_ip) {
    Protocol msg{};
    int index = -1;  // this client's slot in the online[] table

    while (true) {
        int n = recv(client_fd, &msg, sizeof(msg), 0);
        if (n <= 0) {
            if (n == 0)
                std::cout << "Client disconnected: " << client_ip << '\n';
            else
                perror("recv");
            break;
        }

        // If the recv got a partial struct, ignore (simple model)
        if (static_cast<size_t>(n) < sizeof(msg))
            continue;

        switch (msg.cmd) {
        case REGISTE:
            handle_register(client_fd, &index, &msg);
            break;
        case LOGIN:
            handle_login(client_fd, &index, &msg);
            break;
        case BROADCAST:
        case PRIVATE:
        case ONLINEUSER:
            // Not yet implemented (Phase 3+)
            break;
        default:
            break;
        }
    }

    // Cleanup: mark user offline if they were logged in
    del_user_online(index);
    close(client_fd);
}

// ---------- Entry point ----------

//
// @brief  Entry point for the chatroom server.
// @param  argc  Argument count (expected: 2).
// @param  argv  Argument vector — argv[1] is the port number.
// @return 0 on clean shutdown, 1 on error.
//
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " port\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0) {
        std::cerr << "Usage: " << argv[0] << " port\n";
        return 1;
    }

    // Initialize user table — all slots empty
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        online[i].fd = -1;
        online[i].flag = -1;
    }

    // Create listening socket
    ScopedSocket listen_sock(socket(AF_INET, SOCK_STREAM, 0));
    if (listen_sock.get() == -1) {
        perror("socket");
        return 1;
    }

    // Bind to all interfaces
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(listen_sock.get(), reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) == -1) {
        perror("bind");
        return 1;
    }

    // Start listening
    if (listen(listen_sock.get(), 10) == -1) {
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on port " << port << '\n';

    // Accept loop
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

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        std::cout << "Client connected: " << client_ip << '\n';

        // Handle each client in a detached thread
        std::thread(client_handler, client_fd, std::string(client_ip)).detach();
    }
}
