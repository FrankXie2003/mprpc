add_executable(test_protobuf main.cc test.pb.cc)
target_link_libraries(test_protobuf ${Protobuf_LIBRARIES})
