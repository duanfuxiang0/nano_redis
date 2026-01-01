cmake_minimum_required(VERSION 3.16)
project(nano_redis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

find_package(gflags REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error")
set(HOTON_ENABLE_URING ON CACHE BOOL "enable io_uring function" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/photonlibos EXCLUDE_FROM_ALL)

set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/abseil-cpp ${CMAKE_BINARY_DIR}/abseil-cpp EXCLUDE_FROM_ALL)

# Force use of system gtest instead of conda's
set(GTEST_ROOT "/usr/src/googletest" CACHE PATH "Path to googletest source")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest EXCLUDE_FROM_ALL)

# Header files (按功能组织）
set(REDIS_HEADERS
  include/protocol/resp_parser.h
  include/server/server.h
  include/server/connection.h
  include/command/command_registry.h
  include/command/string_family.h
  include/command/hash_family.h
  include/command/set_family.h
  include/command/list_family.h
  include/core/database.h
  include/core/dashtable.h
  include/core/compact_obj.h
  include/core/util.h
)

# Source files (按功能组织）
set(REDIS_SOURCES
  src/server/server.cc
  src/server/connection.cc
  src/protocol/resp_parser.cc
  src/command/command_registry.cc
  src/command/string_family.cc
  src/command/hash_family.cc
  src/command/set_family.cc
  src/command/list_family.cc
  src/core/dashtable.cc
  src/core/database.cc
  src/core/compact_obj.cc
  src/core/util.cc
)

add_library(nano_redis STATIC ${REDIS_SOURCES} ${REDIS_HEADERS})
target_include_directories(nano_redis PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(nano_redis PUBLIC
  absl::strings
  absl::hash
  absl::flat_hash_map
  photon_shared
)

# Server executable
add_executable(nano_redis_server src/server_main.cc)
target_link_libraries(nano_redis_server PRIVATE nano_redis gflags_static)

# Unit tests executable
add_executable(unit_tests
  tests/unit/compact_obj_test.cc
  tests/unit/dashtable_debug_test.cc
  tests/unit/database_test.cc
  tests/unit/dashtable_debug_test.cc
  tests/unit/hash_test.cc
  tests/unit/resp_parser_test.cc
  tests/unit/string_family_test.cc
  tests/unit/hash_family_test.cc
  tests/unit/set_family_test.cc
  tests/unit/list_family_test.cc
)
target_link_libraries(unit_tests PRIVATE
  nano_redis
  GTest::gtest
  GTest::gtest_main
  photon_shared
  gflags_static
)

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
