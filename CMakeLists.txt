cmake_minimum_required(VERSION 3.20)
project(redisClone)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# Server Executable
add_executable(redis-server server.cpp store.cpp persistence.cpp wal.cpp)
target_link_libraries(redis-server ws2_32)
# Benchmark Executable
add_executable(redis-bench benchmark.cpp)
target_link_libraries(redis-bench ws2_32)