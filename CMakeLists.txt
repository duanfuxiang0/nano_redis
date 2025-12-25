cmake_minimum_required(VERSION 3.16)
project(nano_redis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译选项（暂时禁用 -Werror 以兼容 PhotonLibOS）
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

# 查找依赖
find_package(GTest HINTS /usr/lib/x86_64-linux-gnu/cmake/GTest REQUIRED)

# PhotonLibOS - 使用 git submodule
# 禁用 PhotonLibOS 的 Werror 设置
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/photonlibos EXCLUDE_FROM_ALL)

# Abseil - 使用本地目录
set(ABSL_PROPAGATE_CXX_STD ON)

# 设置 Abseil 路径（可通过环境变量覆盖）
if(DEFINED ENV{ABSEIL_PATH})
  set(ABSEIL_ROOT $ENV{ABSEIL_PATH})
else()
  set(ABSEIL_ROOT /home/ubuntu/abseil-cpp)
endif()

add_subdirectory(${ABSEIL_ROOT} ${CMAKE_BINARY_DIR}/abseil-cpp EXCLUDE_FROM_ALL)

# Nano-Redis 库
set(REDIS_SOURCES
  src/version.cc
  src/arena.cc
  src/string_store.cc
  src/string_utils.cc
  src/echo_server.cc
)

set(REDIS_HEADERS
  include/nano_redis/version.h
  include/nano_redis/status.h
  include/nano_redis/arena.h
  include/nano_redis/string_store.h
  include/nano_redis/string_utils.h
  include/nano_redis/test_utils.h
  include/nano_redis/echo_server.h
)

add_library(nano_redis STATIC ${REDIS_SOURCES} ${REDIS_HEADERS})
target_include_directories(nano_redis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nano_redis PUBLIC 
  absl::flat_hash_map
  photon_shared
)

# 测试库（包含测试工具）
add_library(test_utils STATIC tests/test_main.cc)
target_link_libraries(test_utils GTest::gtest GTest::gtest_main)
target_include_directories(test_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 测试
add_executable(version_test tests/version_test.cc)
target_link_libraries(version_test nano_redis test_utils)

add_executable(arena_test tests/arena_test.cc)
target_link_libraries(arena_test nano_redis test_utils)

add_executable(echo_server_test tests/echo_server_test.cc)
target_link_libraries(echo_server_test nano_redis test_utils)

add_executable(test_utils_example_test tests/test_utils_example_test.cc)
target_link_libraries(test_utils_example_test nano_redis test_utils)

enable_testing()
add_test(NAME version_test COMMAND version_test)
add_test(NAME arena_test COMMAND arena_test)
add_test(NAME echo_server_test COMMAND echo_server_test)
add_test(NAME test_utils_example_test COMMAND test_utils_example_test)

# 基准测试（可选）
find_package(benchmark QUIET)
if(benchmark_FOUND)
  add_executable(arena_bench tests/arena_bench.cc)
  target_link_libraries(arena_bench nano_redis benchmark::benchmark)
  
  add_executable(hash_table_bench tests/hash_table_bench.cc)
  target_link_libraries(hash_table_bench nano_redis benchmark::benchmark)
  
  add_executable(string_bench tests/string_bench.cc)
  target_link_libraries(string_bench nano_redis)
endif()
