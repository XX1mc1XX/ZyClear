# 智澈项目顶层构建入口
cmake_minimum_required(VERSION 3.16)

project(zyClear VERSION 2.1.0 LANGUAGES CXX)

set(ZYCLEAR_BIN_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "可执行文件输出目录")

# 单元测试必须在顶层开启：CTest 只从顶层收集 add_test 注册的用例，
# 放在子目录里调用的话 ctest 会报 "No tests were found"。
# include(CTest) 同时定义标准开关 BUILD_TESTING（默认 ON），子目录据此决定是否建测试目标
include(CTest)

add_subdirectory(src)
