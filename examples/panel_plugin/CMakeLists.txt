# =============================================================================
# 示例面板插件
#
# 独立构建的小工程：编出一个 DLL，不需要重新编译主程序，
# 丢进主程序目录下的 extensions/ 就会被自动加载成一个新面板。
#
# 单独编：
#   cmake -S examples/panel_plugin -B build-plugin \
#         -G "Visual Studio 18 2026" -A x64 \
#         -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64"
#   cmake --build build-plugin --config Release
#
# 产物会自动落到 bin/extensions/，重启主程序即可看到「示例插件」面板。
# =============================================================================
cmake_minimum_required(VERSION 3.16)

project(zyclear_panel_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 中文注释与界面文案，MSVC 需要按 UTF-8 解析
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_library(zyclear_panel_demo SHARED
    DemoPanel.cpp
    DemoPanel.h
    plugin_entry.cpp
)

# 插件只需要宿主的一个头文件：插件接口。不链接宿主、不依赖宿主其他代码 ——
# 这正是「可寄生」的意思：换个宿主，插件照样能编。
target_include_directories(zyclear_panel_demo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/Extension"
)

target_link_libraries(zyclear_panel_demo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# 产物直接落到主程序的 extensions/ 目录，省掉手工拷贝这一步
set_target_properties(zyclear_panel_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/extensions"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/extensions"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/extensions"
)
