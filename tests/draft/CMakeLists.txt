# 原型测试的 CMakeLists.txt

# SO_REUSEPORT 测试
add_executable(test_so_reuseport
    test_so_reuseport.cc
)
target_include_directories(test_so_reuseport PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/photonlibos
    ${CMAKE_SOURCE_DIR}/third_party/photonlibos/include
)
target_link_libraries(test_so_reuseport PRIVATE
    pthread
    ${CMAKE_SOURCE_DIR}/third_party/photonlibos/build/libphoton.a
)
·
# TaskQueue 测试
add_executable(test_task_queue
    test_task_queue.cc
)
target_include_directories(test_task_queue PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/photonlibos
    ${CMAKE_SOURCE_DIR}/third_party/photonlibos/include
)
target_link_libraries(test_task_queue PRIVATE
    pthread
    ${CMAKE_SOURCE_DIR}/third_party/photonlibos/build/libphoton.a
)
