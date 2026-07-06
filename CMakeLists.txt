cmake_minimum_required(VERSION 3.10)
project(chatroom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common headers are in the root directory
add_executable(server server.cpp)
target_link_libraries(server PRIVATE pthread)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE pthread)
