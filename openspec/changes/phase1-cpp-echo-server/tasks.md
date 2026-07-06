## 1. Build System

- [x] 1.1 Create `CMakeLists.txt` with C++17, pthread linkage, two targets: `server` and `client`

## 2. Shared Code

- [x] 2.1 Implement `ScopedSocket` RAII wrapper class (shared via header or inline)

## 3. Echo Server

- [x] 3.1 Implement `server.cpp`: socket → bind → listen → accept loop
- [x] 3.2 Implement per-client thread with `std::thread` + lambda: recv → print → send echo back
- [x] 3.3 Handle client disconnect (recv returns 0 or error) without crashing

## 4. Echo Client

- [x] 4.1 Implement `client.cpp`: socket → connect → receive thread + stdin send loop
- [x] 4.2 Use `std::getline` for stdin input to support spaces
- [x] 4.3 Handle server disconnect gracefully

## 5. Build and Verify

- [x] 5.1 Run `cmake -B build && cmake --build build` — compile without errors
- [x] 5.2 Manual smoke test: start server, connect one client, send a message, see echo
- [x] 5.3 Manual smoke test: connect two clients simultaneously, verify independent echo
