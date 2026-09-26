# 智澈 ZyClear

[![build](https://github.com/XX1mc1XX/ZyClear/actions/workflows/build.yml/badge.svg)](https://github.com/XX1mc1XX/ZyClear/actions/workflows/build.yml)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](#)
[![platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)](#构建)

跨品牌工业相机客户端。面向工业视觉产线，覆盖设备发现、接入、调参、配置持久化、
实时成像与多机切换的完整闭环。

设计目标只有一句话：**换品牌改 1 处注册，加型号改 0 行 C++**。

硬件差异被收敛在实现层（约 14% 的代码量），其余部分对品牌完全无感。
内置虚拟相机，**没有真实硬件也能完整跑通全部功能**。

- 语言与框架：C++17 / Qt 6（Widgets · Concurrent · Model-View-Delegate）
- 图像处理：OpenCV 4.x
- 相机 SDK：海康 MVS（MvCameraControl，可选依赖）
- 构建：CMake + MSVC

---

## 功能

| 模块 | 能力 |
|---|---|
| 设备管理 | 枚举、连接、断开、多机切换，连接前判定设备是否被独占 |
| 参数配置 | 六类参数（Int / Float / Enum / Bool / String / Command）读写，权限自动推导 |
| 配置持久化 | 相机配置导入导出，文件类型由适配器上报 |
| 实时成像 | 独立采集线程 + 双队列缓冲，保新弃旧，界面不阻塞 |
| 图像显示 | 缩放、平移、双击自适应、像素级取色 |
| 无硬件调试 | 虚拟相机覆盖全部功能链路 |
| **AI 助手**（可选） | 用日常说法操作相机：说「画面有点暗」，自动查画面指标、改曝光、回读确认 |
| **日志面板** | AI 决策日志 / 相机 SDK 日志 / 应用日志，三份并列成标签页，不用再翻目录 |

---

## 构建

### 依赖

| 依赖 | 版本 | 说明 |
|---|---|---|
| CMake | ≥ 3.16 | |
| Qt | 6.x（Core / Gui / Widgets / Concurrent / Test） | Test 模块供单元测试使用 |
| OpenCV | 4.x | |
| 编译器 | 支持 C++17 的 MSVC | 亦可在 Linux 下用 GCC / Clang |
| 海康 MVS SDK | 可选 | 仓库仅附带头文件（二进制导入库不入库）；缺省时程序自动降级为仅虚拟相机 |

> 海康 SDK 的导入库 `MvCameraControl.lib` 属第三方二进制，未随仓库分发。
> 需要接真机时，从 MVS 安装目录取出该文件放进 `depends/HikCamera/Libraries/`，
> CMake 会自动检测并接入；不放则走降级路径，程序只提供虚拟相机——两条路都能构建成功。

已在下列环境验证构建与单元测试：

| 平台 | 编译器 | Qt | OpenCV |
|---|---|---|---|
| Windows 11 | MSVC 2022 (19.5) | 6.10.1 | 4.5（官方预编译包） |
| Ubuntu 24.04 | GCC 13.3 | 6.4.2（发行版包） | 4.6（发行版包） |

Ubuntu 一行走的是**无海康 SDK** 的降级路径，与 CI 中的配置一致，
说明代码兼容 Qt 6.4 起的全部版本。

### 命令

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
      -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64" \
      -DOpenCV_ROOT="C:/opencv/build"

cmake --build build --config Release
```

可覆盖的缓存变量：

| 变量 | 默认值 | 用途 |
|---|---|---|
| `CMAKE_PREFIX_PATH` | 无 | Qt 6 安装路径 |
| `OpenCV_ROOT` | `C:/opencv/build` | OpenCV 构建目录 |
| `HikSDK_ROOT` | `depends/HikCamera` | 海康 SDK 根目录 |
| `BUILD_TESTING` | `ON` | 是否构建单元测试 |
| `ZYCLEAR_AI_ROOT` | 空 | agent4cpp 安装前缀。留空则不构建 AI 助手，程序照常构建运行（与缺海康 SDK 时同一套降级策略） |

产物统一输出到 `bin/`，与运行时所需的 Qt、OpenCV DLL 同目录。

### 运行

需要让程序找到 Qt 与 OpenCV 的运行时 DLL，二选一：

- 把 `bin/` 所需的 DLL 一并拷入（安装 Qt 时可用 `windeployqt`）
- 或将 Qt 的 `bin` 与 OpenCV 的 `bin` 加入 `PATH`

### 单元测试

```bash
ctest --test-dir build -C Debug --output-on-failure
```

测试只覆盖不含硬件依赖的纯逻辑组件，因此**不需要相机、不需要海康 SDK**。
三个测试目标共 30 个用例：

| 测试 | 用例数 | 覆盖内容 |
|---|---|---|
| `test_cameraimagequeue` | 6 | 取帧超时、保新弃旧、缓冲复用与归还、稳态零分配 |
| `test_cameraparam` | 12 | 六类参数的显示契约、访问权限三态、原型复制、QVariant 装箱 |
| `test_parseuijson` | 12 | Schema 解析、字段校验与错误定位、类型映射、信号契约 |

---

## 无硬件试用

不需要任何相机即可验证全部功能：

1. 启动 `bin/zyClear.exe`
2. 点击工具栏「枚举相机」，列表中出现 `VirtualCamera`
3. 选中它并点击「连接」，右侧参数面板会按 Schema 生成 5 组 37 项参数
4. 点击「拉流」，中间显示区出现实时彩色帧；滚轮缩放、拖动平移、双击自适应
5. 修改任意参数可写回设备，拉流期间参数面板会自动禁用

---

## 架构

分层、依赖规则与扩展方式见 [docs/architecture.md](docs/architecture.md)。

一句话概括：交互层只凭序列号寻址，门面层不认识任何具体适配器，
厂商差异全部落在实现层。三条核心约束都能用 grep 当场验证：

```bash
grep -rn "HikCamera.h\|VirtualCamera.h" src/CameraInterface/   # 期望无输出
grep -rn "ControlWidget\*" src/ParamWidget/ src/ViewWidget/    # 期望无输出
```

### 目录结构

```
├── CMakeLists.txt          顶层构建入口
├── src/
│   ├── CMakeLists.txt      源码清单、依赖探测、资源打包、产物路径
│   ├── CameraInterface/    契约层：相机接口、统一参数模型、图像缓冲队列、统一错误码
│   ├── CameraFactory/      实现层：海康适配器、虚拟相机、工厂注册
│   ├── ControlWidget/      交互层：设备枚举与连接控制栏
│   ├── ParamWidget/        交互层：参数树（模型 / 视图 / 委托 + 六类编辑控件）
│   ├── ViewWidget/         交互层：图像显示、缩放取色、采集线程
│   ├── ParseUiJson/        参数 Schema 解析
│   ├── AppStyle/           主题：调色板 + 样式表
│   ├── LoadingDialog/      加载遮罩
│   ├── Resource/           参数 Schema 等资源
│   ├── Icon/               图标资源
│   ├── Utils/              图像格式转换
│   ├── Extension/          可扩展面板框架（通用层，不含本项目任何业务类型）
│   │   └── Ai/             AI 助手面板：多步循环 / 会话历史 / 知识库
│   ├── Integration/        宿主适配层：把相机能力暴露成 AI 工具（唯一认识相机的扩展点）
│   ├── Listener.*          事件总线
│   ├── mainwindow.*        主窗口装配
│   ├── main.cpp            程序入口
│   └── tests/              单元测试
├── depends/HikCamera/      海康 SDK 头文件与导入库
├── docs/                   架构说明、简历归档
└── .github/workflows/      持续集成
```

---

## 扩展与插件

界面本身也是可扩展的：实现 `IPanel`，再注册一行即可。

```cpp
class MyPanelExtension : public IPanel {
    QString PanelId() const override { return "my.panel"; }
    QString PanelTitle() const override { return "我的面板"; }
    QWidget* CreateWidget(QWidget* parent) override { return new MyPanel(parent); }
};
registry->Register("my.panel", []() -> IPanel* { return new MyPanelExtension(); });
```

更彻底的方式是**外部 DLL 插件**：把面板编成动态库，按约定导出入口，丢进程序目录下的
`extensions/`，启动时自动加载，主程序不需要重新编译：

```cpp
extern "C" ZYCLEAR_EXTENSION_EXPORT int ZyClearExtensionCount();
extern "C" ZYCLEAR_EXTENSION_EXPORT IPanel* ZyClearExtensionAt(int index);
```

**AI 助手与日志面板就是按这个方式接进来的。** 因此 `src/Extension/` 整个目录
不含任何相机相关代码，可以整包搬到其他上位机（PLC、运动控制卡、测试台）：
新宿主只需实现一个 `IToolProvider`（把已有业务函数包成工具，几十行），
面板、会话历史、知识库、多步循环全部复用。

依赖方向是单向的：**宿主 → 接口 ← 通用层**。通用层不反向依赖宿主。

---

## 扩展

**新增相机品牌**：继承 `CameraInterface` 实现 17 个纯虚方法，
在 `CameraFactory` 里追加一行 `registerVendor<新适配器>("厂商名")`。
门面、参数面板、图像链路均无需改动。

**新增相机型号**：修改 `src/Resource/VirtualCameraParam.json` 即可，不编译、不发版。

详见 [docs/architecture.md](docs/architecture.md#六扩展点)。

---

## 许可证

[MIT](LICENSE)

本项目的相机适配层依赖于海康机器人 MVS SDK，该 SDK 的著作权与许可条款归其发布方所有，
不在本项目许可证覆盖范围内；使用前请自行确认并遵守其许可要求。
