# 设置cmake的最低版本要求
cmake_minimum_required(VERSION 3.10)
# 设置项目名称和版本
project(mprpc)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)

# 设置输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/src/include)
include_directories(${PROJECT_SOURCE_DIR}/example)

# 查找protobuf包
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})

# 添加子目录
# src包含了mprpc框架的源代码
add_subdirectory(src)
# add_subdirectory(test)
# example包含了mprpc框架是用的示例代码
add_subdirectory(example)