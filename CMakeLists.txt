# 智澈项目顶层构建入口
cmake_minimum_required(VERSION 3.16)

project(zyClear VERSION 2.1.0 LANGUAGES CXX)

set(ZYCLEAR_BIN_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "可执行文件输出目录")

# CTest 只从顶层收集用例，放子目录里会报 "No tests were found"。
# include(CTest) 同时定义开关 BUILD_TESTING（默认 ON）
include(CTest)

add_subdirectory(src)
