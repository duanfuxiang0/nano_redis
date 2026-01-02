# CMake generated Testfile for 
# Source directory: /home/ubuntu/nano_redis
# Build directory: /home/ubuntu/nano_redis/build_debug
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_tests "/home/ubuntu/nano_redis/build_debug/unit_tests")
set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntu/nano_redis/CMakeLists.txt;112;add_test;/home/ubuntu/nano_redis/CMakeLists.txt;0;")
subdirs("third_party/photonlibos")
subdirs("abseil-cpp")
subdirs("third_party/googletest")
