# 单元测试。
#
# 被测对象刻意只选不含硬件依赖的纯逻辑组件——图像缓冲队列、参数模型、JSON 解析、
# 事件总线、相机工厂与门面——因此测试不需要相机、不需要海康 SDK，
# 在任何装了 Qt 的环境里都能编译并跑通。
#
# 每个用例文件单独成一个可执行文件：QtTest 的 QTEST_MAIN 宏会在每个用例源文件里
# 各生成一个 main，合并进同一目标必然多重定义。
find_package(Qt6 REQUIRED COMPONENTS Test Gui Widgets)

# OpenCV 探测：优先官方包，缺失时回退到 OpenCV_ROOT 下的手动路径，
# 与主工程的探测逻辑保持一致。放在最前，供下面的 helper 与各测试目标共用。
set(OpenCV_ROOT "C:/opencv/build" CACHE PATH "OpenCV 构建根目录")
find_package(OpenCV QUIET PATHS ${OpenCV_ROOT})

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

    # -o -,txt：显式要求 QtTest 把用例级结果写进标准输出。
    # 不加这个，Windows 上没接控制台时结果会被丢弃，ctest 只说一句 Failed，
    # 看不到究竟是哪条用例、哪个值不对。
    add_test(NAME ${test_target} COMMAND ${test_target} -o -,txt)
endfunction()

# 给测试目标接上 OpenCV。用到 cv::Mat 的目标都得走这个函数，
# 免得 OpenCV 的探测分支在每处各抄一份
function(zyclear_link_opencv test_target)
    if(OpenCV_FOUND)
        target_link_libraries(${test_target} PRIVATE ${OpenCV_LIBS})
    else()
        target_include_directories(${test_target} PRIVATE "${OpenCV_ROOT}/include")
        target_link_directories(${test_target} PRIVATE "${OpenCV_ROOT}/x64/vc15/lib")
        target_link_libraries(${test_target} PRIVATE
            $<$<CONFIG:Debug>:opencv_world455d>
            $<$<NOT:$<CONFIG:Debug>>:opencv_world455>
        )
    endif()
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
zyclear_link_opencv(test_cameraimagequeue)

# 事件总线。总线是进程级单例、无反注册接口，用例只做投递与计数，
# 订阅者用文件级静态对象持有，活到进程结束，不会留下野指针。
# Listener 只依赖 QtCore，不涉及图像，故不需要 OpenCV
zyclear_add_test(test_listener
    test_listener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Listener.cpp
)

# 相机工厂。本目标不定义 ZYCLEAR_HAS_HIK_SDK，工厂因而只注册虚拟相机品牌，
# 测试与真机、与厂商 SDK 彻底无关。虚拟相机经 CameraImageQueue 持有 cv::Mat，
# 故需接 OpenCV
zyclear_add_test(test_camerafactory
    test_camerafactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CameraFactory/CameraFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CameraFactory/VirtualCamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CameraInterface/CameraImageQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ParseUiJson/ParseUiJson.cpp
)
zyclear_link_opencv(test_camerafactory)

# 门面层。相机对象、参数骨架与图像缓冲都要经它走一遍，因此依赖链最长：
# 门面 → 工厂 → 虚拟相机 → JSON Schema。参数骨架来自 Qt 资源，
# 故把 resource.qrc 一并编进本目标，测试才能读到 :/VirtualCameraParam.json。
# CameraContext.cpp 顶部包含 QApplication 头，所以这里必须接上 QtWidgets
zyclear_add_test(test_cameracontext
    test_cameracontext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CameraInterface/CameraContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CameraFactory/CameraFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CameraFactory/VirtualCamera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CameraInterface/CameraImageQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ParseUiJson/ParseUiJson.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Resource/resource.qrc
)
target_link_libraries(test_cameracontext PRIVATE Qt6::Gui Qt6::Widgets)
zyclear_link_opencv(test_cameracontext)

# 参数模型。模型接口本身属 QtCore，但实现文件里包含了 QtWidgets，
# 故需接上该模块；测试用 GUILESS 入口，不构造 QApplication。
# 被测的是模型语义，参数对象就地造，不经过相机
zyclear_add_test(test_cameraparammodel
    test_cameraparammodel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ParamWidget/CameraParamModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ParamWidget/CameraParamItem.cpp
)
target_link_libraries(test_cameraparammodel PRIVATE Qt6::Widgets)

# 图像转换桥。头文件即实现，没有单独的 .cpp 要编；
# 被测的是像素排布与借用语义，用就地造出来的 cv::Mat 与 QImage
zyclear_add_test(test_imageconver
    test_imageconver.cpp
)
target_link_libraries(test_imageconver PRIVATE Qt6::Gui)
zyclear_link_opencv(test_imageconver)
