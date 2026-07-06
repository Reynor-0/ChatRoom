//
// protocol.h
// Shared communication protocol between server and client.
// Defines the wire format (struct Protocol), command types, return codes,
// and server-side user tracking structure.
//
// Usage:
//   #include "protocol.h"
//   Both server.cpp and client.cpp include this header.
//

#pragma once

#include <cstdint>

// ---------- Protocol constants ----------

constexpr int SERVER_PORT = 8888;
constexpr int MAX_USER_NUM = 64;

// ---------- Online user record (server-side) ----------

//
// @brief  Server-side record for a registered user.
//
// Tracks registration data and current connection status.
//
struct OnlineUser {
    int  fd;              // socket fd, -1 means offline
    int  flag;            // -1: empty slot, 1: has registration data
    char name[32];        // username
    char passwd[32];      // password (plaintext for now)
};

// ---------- Wire protocol structure ----------

//
// @brief  Message exchanged between client and server.
//
// All communication uses this fixed-size binary struct.
//
struct Protocol {
    int  cmd;             // command type (see command codes below)
    int  state;           // return / status code
    char name[32];        // username (or target username)
    char data[64];        // password, message payload, or system text
};

// ---------- Command codes (client → server) ----------

constexpr int BROADCAST   = 0x00000001;  // broadcast to all users
constexpr int PRIVATE     = 0x00000002;  // private message to one user
constexpr int REGISTE     = 0x00000004;  // register a new account
constexpr int LOGIN       = 0x00000008;  // login with credentials
constexpr int ONLINEUSER  = 0x00000010;  // list online users
constexpr int LOGOUT      = 0x00000020;  // logout / disconnect

// ---------- Return codes (server → client) ----------

constexpr int OP_OK              = 0x80000000;  // operation succeeded
constexpr int ONLINEUSER_OK      = 0x80000001;  // online-user list entry
constexpr int ONLINEUSER_OVER    = 0x80000002;  // online-user list done
constexpr int NAME_EXIST         = 0x80000003;  // registration: name taken
constexpr int NAME_PWD_NMATCH    = 0x80000004;  // login: wrong credentials
constexpr int USER_LOGED         = 0x80000005;  // login: already online
constexpr int USER_NOT_REGIST    = 0x80000006;  // login: not registered
