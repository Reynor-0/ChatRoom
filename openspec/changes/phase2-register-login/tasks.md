## 1. Protocol Header

- [x] 1.1 Create `protocol.h` with `struct Protocol`, `struct OnlineUser`, command codes, and return codes

## 2. Server

- [x] 2.1 Add user storage (`std::array<OnlineUser, 64>`) and init in `main()`
- [x] 2.2 Implement `find_user()` — search registered users by name
- [x] 2.3 Implement `find_user_online()` — search by name+password for login
- [x] 2.4 Implement `handle_register()` — add user or return NAME_EXIST
- [x] 2.5 Implement `handle_login()` — verify credentials, set fd, or return error code
- [x] 2.6 Implement `del_user_online()` — mark user offline on disconnect
- [x] 2.7 Rewrite client handler thread: recv `Protocol`, dispatch by `cmd`

## 3. Client

- [x] 3.1 Implement `do_register()` — prompt name/passwd, send REGISTE, print result
- [x] 3.2 Implement `do_login()` — prompt name/passwd, send LOGIN, set login state
- [x] 3.3 Implement menu loop: show different menus based on `login_f` state
- [x] 3.4 Add background receive thread that prints server responses
- [x] 3.5 Add `do_logout()` — close connection and reset login state

## 4. Build and Verify

- [x] 4.1 Run `cmake --build build` — compile without errors
- [x] 4.2 Test: register a new user, verify success
- [x] 4.3 Test: register duplicate name, verify rejection
- [x] 4.4 Test: login with correct credentials, verify success
- [x] 4.5 Test: login with wrong password, verify rejection
