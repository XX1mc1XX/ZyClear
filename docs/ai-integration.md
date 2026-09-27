# AI 助手集成设计

给 zyClear 加一层「自然语言控相机」的能力：用户输入一句话，AI 自己决定调哪些参数、传什么值。

**这一层是纯增量**：不碰契约层、实现层、现有交互层。删掉它就是原来的 zyClear。

---

## 一、一句话原理

```
用户："画面有点暗"
  → AI 面板把问题 + 工具清单交给大模型
  → 大模型回："先调 get_frame_stats 看看"
  → 程序真的调了 OpenCV 算亮度和过曝占比
  → 结果回灌给大模型
  → 大模型回："调 set_param('ExposureTime', 8000)"
  → 程序真的写了相机参数
  → 大模型总结成一句人话
```

**大模型不执行任何代码**，它只输出「我要调谁、传什么」。真正碰相机的是 `CameraContext`。

---

## 二、这一层插在哪

```
交互层   ControlWidget │ ParamWidget │ ViewWidget │ ★ AiPanel
                                              │
                                     ★ AiAgentService（后台线程跑循环）
                                              │
                                     ★ CameraToolProvider（适配层：Qt 类型 ↔ JSON）
                                              ▼
门面层   CameraContext  ← AI 只认这一层，凭序列号寻址
```

**约束**：AI 层只允许调 `CameraContext`，不得出现任何厂商类型，
也不得直接调 `CameraInterface`。与现有架构的依赖规则一致。

---

## 三、工具清单（10 个）

工具粒度选「通用工具 + 参数名」而不是「一个参数一个工具」——
37 个参数会变成 37 个工具，模型反而选不准。

| 工具 | 参数 | 语义化返回值 |
|---|---|---|
| `list_cameras` | — | 序列号 / 品牌 / 是否连接 / 是否拉流 |
| `get_current_camera` | — | 当前选中相机的序列号 |
| `connect_camera` | `serial?` | 连接结果 |
| `disconnect_camera` | `serial?` | 断开结果 |
| `list_params` | `group?` | 参数名 / 当前值 / 可读可写 / 取值范围 |
| `get_param` | `name` | 单个参数的值与范围 |
| `set_param` | `name`, `value` | 写入结果 + **写后回读的实际值** |
| `start_grab` / `stop_grab` | `serial?` | 拉流状态 |
| `get_frame_stats` | `serial?` | ★ 亮度 / 过曝占比 / 欠曝占比 / 对比度 / 清晰度 + **语义判定** |

### ★ 为什么 `get_frame_stats` 是关键

**大模型看不到图像**。不能问它「画面暗不暗」——它只会瞎猜。

必须先用 OpenCV 把图像变成**数字**，再让模型基于数字推理：

| 指标 | 算法 | 给模型的语义字段 |
|---|---|---|
| 平均亮度 | `cv::mean` | `brightness: 0.31` |
| 过曝占比 | 灰度 > 250 的像素比例 | `overexposed_ratio: 0.02` |
| 欠曝占比 | 灰度 < 20 的像素比例 | `underexposed_ratio: 0.41` |
| 对比度 | 灰度标准差 | `contrast: 18.3` |
| 清晰度 | Laplacian 方差 | `sharpness: 12.7` |
| **判定** | 上述指标合成 | `assessment: "underexposed"` |

`assessment` 那句人话判定就是「语义化观测值」——**有了它，模型不用自己猜
「brightness 0.31 算不算暗」**。

---

## 四、线程模型（最容易出错的地方）

```
主线程（UI）                          后台线程
────────────                        ────────────
点击「发送」
  └─ AiAgentService::ask()  ──────→  QtConcurrent::run
                                        agent4cpp::Agent::Run()
                                          └─ 循环问模型
                                          └─ 调工具 ──┐
                                                       │
  ←──── BlockingQueuedConnection ──────────────────────┘
  真正操作相机（主线程执行）
  └─ 结果返回后台线程
                                        Run() 返回
  ←──── 信号 finished(答复) ────────────┘
```

**为什么工具要切回主线程**：`CameraContext` 和它背后的 `CameraInterface`
都不是线程安全的，而且用户可能同时在点界面。所有相机操作串行在 UI 线程，
天然避免并发冲突。

> 代价：AI 调参数期间界面事件队列被占用。但因为每次工具调用很短（毫秒级），
> 而耗时的部分（等模型回复）在后台线程，所以界面不会卡。

---

## 五、配置与安全

| 项 | 做法 | 理由 |
|---|---|---|
| API Key | `QSettings` 存本机（注册表），**绝不进源码/仓库** | Key 泄漏 = 别人拿你的账号花钱 |
| Key 传给 agent4cpp | `qputenv()` 注入环境变量 | agent4cpp 只从环境变量读 Key（`api_key_env` 是变量名不是 Key） |
| Base URL / 模型名 | 设置对话框可改，默认 DeepSeek | 换服务商不改代码 |
| 首次使用 | 未配置时禁用发送按钮，提示先配置 | 避免用户对着没反应的按钮骂人 |

默认值：

| 服务 | base_url | model |
|---|---|---|
| DeepSeek | `https://api.deepseek.com/v1` | `deepseek-chat` |
| 智谱 GLM | `https://open.bigmodel.cn/api/paas/v4` | `glm-4-plus` |
| 本地 Ollama | `http://localhost:11434/v1` | `qwen2.5` |

---

## 六、可选依赖（照抄海康 SDK 的模式）

agent4cpp 是**闭源商业授权**的第三方库，zyClear 是 MIT 开源项目。

| 规则 | 做法 |
|---|---|
| **不入库** | agent4cpp 的 dll / lib / 头文件**不提交进本仓库**（与 `MvCameraControl.lib` 同样处理） |
| **可选** | `find_package(agent4cpp CONFIG QUIET)`，找不到时 `AiAgent/` 整体不参与编译，程序退化为纯相机客户端 |
| **不硬编码路径** | 用 `-DZYCLEAR_AI_ROOT=<安装前缀>` 指定，默认空 |
| **降级要能构建** | 没有 agent4cpp 时 `cmake --build` 必须照样成功 |

配置命令：

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 \
      -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/msvc2022_64" \
      -DOpenCV_ROOT="C:/opencv/build" \
      -DZYCLEAR_AI_ROOT="<agent4cpp 安装前缀>"
```

---

## 七、文件清单（全部新增，不改现有文件）

| 文件 | 职责 | 行数估 |
|---|---|---|
| `src/AiAgent/AiConfig.h/.cpp` | 配置读写（QSettings）+ Key 环境变量注入 | ~120 |
| `src/AiAgent/CameraToolProvider.h/.cpp` | 工具适配层：Qt 类型 ↔ JSON，跨线程切回主线程 | ~420 |
| `src/AiAgent/AiAgentService.h/.cpp` | 持有 Agent，后台线程跑循环，信号回 UI | ~150 |
| `src/AiAgent/AiPanel.h/.cpp` | 聊天面板（消息列表 + 输入框 + 设置入口） | ~260 |
| `src/AiAgent/AiSettingsDialog.h/.cpp` | Base URL / Key / 模型 配置对话框 | ~150 |
| `src/AiAgent/AiModule.h` | 编译开关（`ZYCLEAR_HAS_AI`） | ~20 |

改动的现有文件（**仅 3 处**）：

| 文件 | 改动 |
|---|---|
| `src/CMakeLists.txt` | 加可选依赖探测块 + 源文件加入清单 |
| `src/mainwindow.h/.cpp` | 两处：splitter 的宽度分配，以及调一次 `ExtensionHost::Attach()` 把面板挂成可停靠面板 |
| `README.md` | 加一节 AI 功能说明 + 可选依赖说明 |

**现有 27 个编译单元一行不改**，架构的依赖规则不被破坏。
