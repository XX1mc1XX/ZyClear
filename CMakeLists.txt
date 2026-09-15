# 智澈项目顶层构建入口
cmake_minimum_required(VERSION 3.16)

project(zyClear VERSION 2.1.0 LANGUAGES CXX)

set(ZYCLEAR_BIN_DIR "${CMAKE_SOURCE_DIR}/../bin" CACHE PATH "可执行文件输出目录")

add_subdirectory(src)
