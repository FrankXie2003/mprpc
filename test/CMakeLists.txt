add_subdirectory(protobuf)

# zookeeper 测试
add_executable(test_zk test_zk.cc)
target_link_libraries(test_zk mprpc muduo_net muduo_base pthread zookeeper_mt)
