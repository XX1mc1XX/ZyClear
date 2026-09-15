# 单元测试。
#
# 被测对象刻意只选不含硬件依赖的纯逻辑组件——图像缓冲队列、参数模型、JSON 解析——
# 因此测试不需要相机、不需要海康 SDK，在任何装了 Qt 的环境里都能编译并跑通。
#
# 每个用例文件单独成一个可执行文件：QtTest 的 QTEST_MAIN 宏会在每个用例源文件里
# 各生成一个 main，合并进同一目标必然多重定义。
find_package(Qt6 REQUIRED COMPONENTS Test)

# 测试产物与主程序同样落到 bin 目录：那里 Qt 与 OpenCV 的运行时 DLL 齐备，
# 测试可直接运行，无需另外配置 PATH
function(zyclear_add_test test_target)
    add_executable(${test_target} ${ARGN})

    # 与主程序一致的包含根（src），保证源码里 "CameraInterface/xxx.h" 这类写法可解析
    target_include_directories(${test_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

    target_link_libraries(${test_target} PRIVATE
        Qt6::Core
        Qt6::Test
    )

    set_target_properties(${test_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${ZYCLEAR_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ZYCLEAR_BIN_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ZYCLEAR_BIN_DIR}"
    )

    add_test(NAME ${test_target} COMMAND ${test_target})
endfunction()

# 参数模型是 header-only，直接包含头文件即可测试
zyclear_add_test(test_cameraparam
    test_cameraparam.cpp
)

zyclear_add_test(test_parseuijson
    test_parseuijson.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ParseUiJson/ParseUiJson.cpp
)

zyclear_add_test(test_cameraimagequeue
    test_cameraimagequeue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CameraInterface/CameraImageQueue.cpp
)

# 图像缓冲队列用到 cv::Mat，取与主工程同款的 OpenCV 探测逻辑
set(OpenCV_ROOT "C:/opencv/build" CACHE PATH "OpenCV 构建根目录")
find_package(OpenCV QUIET PATHS ${OpenCV_ROOT})
if(OpenCV_FOUND)
    target_link_libraries(test_cameraimagequeue PRIVATE ${OpenCV_LIBS})
else()
    target_include_directories(test_cameraimagequeue PRIVATE "${OpenCV_ROOT}/include")
    target_link_directories(test_cameraimagequeue PRIVATE "${OpenCV_ROOT}/x64/vc15/lib")
    target_link_libraries(test_cameraimagequeue PRIVATE
        $<$<CONFIG:Debug>:opencv_world455d>
        $<$<NOT:$<CONFIG:Debug>>:opencv_world455>
    )
endif()
