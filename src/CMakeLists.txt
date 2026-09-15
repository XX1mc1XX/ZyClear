# 主工程构建脚本：描述 zyClear 可执行文件由哪些源码编出来、依赖哪些库、编完放哪里
cmake_minimum_required(VERSION 3.16)

project(zyClear LANGUAGES CXX)

# 全工程统一用 C++17。REQUIRED ON 表示编译器不支持就直接报错，不偷偷降级
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# AUTOMOC：Qt 的信号槽、Q_OBJECT 等需要 moc 预处理，打开后 CMake 自动处理，不用手写规则
# AUTORCC：.qrc 资源文件（图标、样式表、json）打开后自动编进 exe，不用手动跑 rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 找 Qt6，缺任何一个模块都直接报错停下。四个模块分工：
# Core（基础：字符串、容器、信号槽）、Gui（绘图、事件、图像）、
# Widgets（按钮、窗口等界面控件）、Concurrent（后台线程跑耗时任务，不卡界面）
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# MSVC 默认按系统本地编码读源码，中文注释和界面文案会乱码，这行强制按 UTF-8 解析
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# 参与编译的全部 .cpp 清单。目录即分工：
# AppStyle（界面样式）、CameraFactory（海康相机和虚拟相机实现）、
# CameraInterface（相机抽象层：连接管理、图像队列）、ControlWidget（顶部控制栏）、
# LoadingDialog（加载弹窗）、ParamWidget（含 CustomWidget：按 bool/int/double/enum 等类型生成的参数控件）、
# ParseUiJson（虚拟相机参数解析）、ViewWidget（图像显示：视图、图元、采图处理）、根目录（程序入口和主窗口）
set(ZYCLEAR_SOURCES
    AppStyle/AppStyle.cpp
    CameraFactory/CameraFactory.cpp
    CameraInterface/CameraContext.cpp
    CameraInterface/CameraImageQueue.cpp
    CameraFactory/HikCamera.cpp
    CameraFactory/VirtualCamera.cpp
    ControlWidget/ControlWidget.cpp
    Listener.cpp
    LoadingDialog/LoadingDialog.cpp
    ParamWidget/CameraParamDelegate.cpp
    ParamWidget/CameraParamItem.cpp
    ParamWidget/CameraParamModel.cpp
    ParamWidget/CustomWidget/BoolCustomWidget.cpp
    ParamWidget/CustomWidget/CmdCustomWidget.cpp
    ParamWidget/CustomWidget/DoubleCustomWidget.cpp
    ParamWidget/CustomWidget/EnumCustomWidget.cpp
    ParamWidget/CustomWidget/IntCustomWidget.cpp
    ParamWidget/CustomWidget/StringCustomWidget.cpp
    ParamWidget/OneCustomWidget.cpp
    ParamWidget/ParamWidget.cpp
    ParseUiJson/ParseUiJson.cpp
    ViewWidget/AcquireImageProcess.cpp
    ViewWidget/GraphicsView.cpp
    ViewWidget/ImageItem.cpp
    ViewWidget/ViewWidget.cpp
    main.cpp
    mainwindow.cpp
)

# 要打进 exe 的三组资源：界面样式表（style.qrc）、按钮图标（Icon.qrc）、虚拟相机参数等杂项（resource.qrc）
set(ZYCLEAR_RESOURCES
    AppStyle/style.qrc
    Icon/Icon.qrc
    Resource/resource.qrc
)

# 头文件显式列入 target：对构建无影响，Qt Creator 按 target sources 建项目树，不加则头文件不显示
set(ZYCLEAR_HEADERS
    AppStyle/AppStyle.h
    CameraFactory/CameraFactory.h
    CameraFactory/HikCamera.h
    CameraFactory/VirtualCamera.h
    CameraInterface/CameraContext.h
    CameraInterface/CameraError.h
    CameraInterface/CameraImageQueue.h
    CameraInterface/CameraInterface.h
    CameraInterface/ZCCameraMetaInfo.h
    CameraInterface/ZCCameraParam.h
    ControlWidget/ControlWidget.h
    Listener.h
    LoadingDialog/LoadingDialog.h
    ParamWidget/CameraParamDelegate.h
    ParamWidget/CameraParamItem.h
    ParamWidget/CameraParamModel.h
    ParamWidget/CustomWidget/BoolCustomWidget.h
    ParamWidget/CustomWidget/CmdCustomWidget.h
    ParamWidget/CustomWidget/DoubleCustomWidget.h
    ParamWidget/CustomWidget/EnumCustomWidget.h
    ParamWidget/CustomWidget/IntCustomWidget.h
    ParamWidget/CustomWidget/StringCustomWidget.h
    ParamWidget/OneCustomWidget.h
    ParamWidget/ParamWidget.h
    ParseUiJson/ParseUiJson.h
    Utils/ImageConver.h
    ViewWidget/AcquireImageProcess.h
    ViewWidget/GraphicsView.h
    ViewWidget/ImageItem.h
    ViewWidget/ViewWidget.h
    mainwindow.h
)

# Windows 下把 appicon.rc 编进去，任务栏和资源管理器里 exe 才有自定义图标；Linux 下不需要所以只在 WIN32 加
if(WIN32)
    list(APPEND ZYCLEAR_SOURCES Resource/appicon.rc)
endif()

# 生成 zyClear.exe。WIN32 表示 GUI 程序，启动时不带黑控制台窗口
add_executable(zyClear WIN32
    ${ZYCLEAR_SOURCES}
    ${ZYCLEAR_HEADERS}
    ${ZYCLEAR_RESOURCES}
)

# 头文件搜索根目录设为当前目录，这样源码里 #include "ViewWidget/GraphicsView.h" 这类按目录写的引用能找到
target_include_directories(zyClear PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 链接 Qt 库。PRIVATE 表示这些依赖只自己用，不传递给别人（exe 已是最终产物，写 PRIVATE 最合适）
target_link_libraries(zyClear PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
)

# OpenCV 用来做图像处理（格式转换、算法等）。装在别的路径时，配置那一步传 -DOpenCV_ROOT=你的路径 即可覆盖下面默认值
set(OpenCV_ROOT "C:/opencv/build" CACHE PATH "OpenCV 构建根目录")
# 先试官方标准方式找（Config 包）；QUIET 表示找不到也不刷屏报错，后面有兜底；NO_DEFAULT_PATH 表示只在上面指定路径找，不用系统杂路径
find_package(OpenCV QUIET PATHS ${OpenCV_ROOT} NO_DEFAULT_PATH)
if(OpenCV_FOUND)
    target_link_libraries(zyClear PRIVATE ${OpenCV_LIBS})
else()
    # 兜底：标准方式找不到就手动指定头文件和库目录。Debug 链接带 d 后缀的库（opencv_world455d），Release 链接不带 d 的，$<CONFIG:Debug> 会按当前配置自动二选一
    target_include_directories(zyClear PRIVATE "${OpenCV_ROOT}/include")
    target_link_directories(zyClear PRIVATE "${OpenCV_ROOT}/x64/vc15/lib")
    target_link_libraries(zyClear PRIVATE
        $<$<CONFIG:Debug>:opencv_world455d>
        $<$<NOT:$<CONFIG:Debug>>:opencv_world455>
    )
endif()

# 海康相机 SDK（枚举、连接、采图全靠它）。默认指向版本根 depends/HikCamera，换位置时传 -DHikSDK_ROOT=你的路径 覆盖
set(HikSDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../depends/HikCamera" CACHE PATH "海康相机 SDK 根目录")
# 有头文件和库目录才接SDK；没有就只给个警告继续编（此时只能用虚拟相机，真机相关代码编不过会报错，见 HikCamera.cpp）
if(EXISTS "${HikSDK_ROOT}/Includes" AND EXISTS "${HikSDK_ROOT}/Libraries")
    target_include_directories(zyClear PRIVATE "${HikSDK_ROOT}/Includes")
    target_link_directories(zyClear PRIVATE "${HikSDK_ROOT}/Libraries")
    target_link_libraries(zyClear PRIVATE MvCameraControl)
else()
    message(WARNING "未找到海康 SDK（${HikSDK_ROOT}），已跳过其头文件与链接配置")
endif()

# 不管 Debug 还是 Release，exe 统一输出到版本根 bin 目录（ZYCLEAR_BIN_DIR 在顶层 CMakeLists 里定义）。
# 这样双击运行和 F5 调试永远只认 bin 这一个地方，不用来回找 exe
set_target_properties(zyClear PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ZYCLEAR_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ZYCLEAR_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ZYCLEAR_BIN_DIR}"
)

# Linux 打包安装规则：make install 时把程序装到 /opt/zyClear/bin，Windows 下这段不生效
if(UNIX AND NOT ANDROID)
    include(GNUInstallDirs)
    install(TARGETS zyClear DESTINATION /opt/zyClear/bin)
endif()
