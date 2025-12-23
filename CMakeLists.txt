cmake_minimum_required(VERSION 3.16)
project(nano_redis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
endif()

# 查找依赖
find_package(GTest REQUIRED)

# Nano-Redis 库
set(REDIS_SOURCES
  src/version.cc
)

set(REDIS_HEADERS
  include/nano_redis/version.h
  include/nano_redis/status.h
)

add_library(nano_redis STATIC ${REDIS_SOURCES} ${REDIS_HEADERS})
target_include_directories(nano_redis PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 测试
add_executable(version_test tests/version_test.cc)
target_link_libraries(version_test nano_redis GTest::gtest GTest::gtest_main)

enable_testing()
add_test(NAME version_test COMMAND version_test)

# 基准测试（后续阶段）
# add_executable(arena_bench tests/arena_bench.cc)
# target_link_libraries(arena_bench nano_redis benchmark::benchmark)
