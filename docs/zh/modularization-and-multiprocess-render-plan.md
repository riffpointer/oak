# Olive/Oak 模块化与多进程渲染架构方案

> **状态**：设计文档（Design Doc）  
> **范围**：仅制定方案，不涉及代码变更。  
> **目标**：将当前单体架构拆分为多个动态库，并将渲染引擎改造为独立进程，通过 stdio 进行 IPC 通信。

---

## 1. 概述

当前 Olive/Oak 采用**单体编译模型**：所有业务代码被聚合到 `libolive-editor`（OBJECT 库），最终链接为单个 `olive-editor` 可执行文件。这种架构在项目规模较小时工作良好，但随着 OFX 插件、节点图复杂度、多轨道高清/超高清处理的加入，单体架构面临以下问题：

- **编译-链接耗时**：任何小改动都触发大规模重编译和重链接。
- **渲染崩溃导致编辑器全崩**：OpenGL/OFX/FFmpeg 的崩溃会直接拖垮整个 GUI 进程，用户未保存的工作全部丢失。
- **插件隔离性差**：OFX 插件与主程序共享地址空间，恶意或 buggy 插件可任意破坏内存。
- **可扩展性受限**：未来如要支持分布式渲染、云渲染、独立批处理工具，均需先打破单体边界。

本方案提出**两阶段架构演进**：

1. **动态库拆分**：按功能层次将代码拆分为若干共享库（`.so`/`.dylib`/`.dll`），明确模块边界与符号可见性。
2. **渲染器多进程化**：将 `render/` 相关逻辑从主进程剥离为独立可执行文件 `olive-renderer`，主进程通过 **stdio（stdin/stdout）** 进行 IPC 控制，**共享内存/内存映射文件**传输大帧数据。

---

## 2. 总体架构目标

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         olive-editor（主进程，GUI）                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │   liboliveui │  │ libolivenode │  │libolivecodec │  │ liboliverender  │  │
│  │  (widget/    │  │  (node/      │  │  (codec/     │  │  -client (轻量) │  │
│  │   panel/     │  │   timeline/  │  │   common/)   │  │   IPC 封装层    │  │
│  │   window/)   │  │   undo/)     │  │              │  │                 │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  └────────┬────────┘  │
│         ▲                 ▲                 ▲                    │           │
│         └─────────────────┴─────────────────┘                    │ QProcess  │
│                              动态链接                            │ stdin/stdout│
├──────────────────────────────────────────────────────────────────┼───────────┤
│                                                                  │           │
│  ┌───────────────────────────────────────────────────────────────┘           │
│  │   olive-renderer（子进程，无 GUI）                                          │
│  │  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │  │ RenderService (IPC 服务端，监听 stdin，输出 stdout)                   │   │
│  │  └──────────────────────────┬──────────────────────────────────────────┘   │
│  │                             │                                              │
│  │  ┌──────────────────────────▼──────────────────────────────────────────┐   │
│  │  │ RenderProcessor + OpenGLRenderer + DecoderCache + ShaderCache        │   │
│  │  │ (原有渲染逻辑，在独立地址空间运行，崩溃不影响主进程)                    │   │
│  │  └─────────────────────────────────────────────────────────────────────┘   │
│  └────────────────────────────────────────────────────────────────────────────┘
│
│  基础依赖（所有模块共享）：
│  ┌─────────────┐    ┌─────────────┐    ┌──────────────────┐
│  │ libolivecore│    │liboliveaudio│    │  liboliveplugin  │
│  │ (ext/core)  │    │  (audio/)   │    │ (pluginSupport/) │
│  └─────────────┘    └─────────────┘    └──────────────────┘
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 动态库拆分方案

### 3.1 拆分原则

- **低侵入性**：优先拆分依赖关系清晰、接口明确的模块；对耦合严重的 `node/` ↔ `render/` 暂不强行物理分割，而是通过**接口抽象 + 动态链接**降低耦合。
- **分层依赖**：严格遵循 `上层 → 下层` 的依赖方向，禁止循环依赖。
- **符号可控**：引入 `OLIVE_<MODULE>_API` 宏，显式导出公共接口，隐藏内部符号（`-fvisibility=hidden`）。
- **Qt 元对象系统兼容**：跨动态库的 Qt 信号/槽需确保 `moc` 生成的元对象信息可被正确链接，推荐在公共头文件中完整声明信号/槽。

### 3.2 库划分

| 动态库 | 包含源码 | 外部依赖 | 说明 |
|---|---|---|---|
| `libolivecore.so` | `ext/core/` | FFmpeg::avutil, OpenGL::GL, Imath | 已有独立库，仅将构建类型由 `STATIC` 改为 `SHARED`。 |
| `libolivecodec.so` | `app/codec/`, `app/common/` | olivecore, FFmpeg, OpenImageIO, OpenEXR | 编解码 + 通用工具。`common/` 因被 `codec/` 重度依赖且不含 UI，故合并。 |
| `liboliveplugin.so` | `app/pluginSupport/`, `third_party/openfx/HostSupport` | olivecore, Qt::Core, expat | OFX 宿主支持，相对独立。 |
| `liboliveaudio.so` | `app/audio/` | olivecore, PortAudio, Qt::Core | 音频播放管理。 |
| `libolivenode.so` | `app/node/`, `app/timeline/`, `app/undo/`, `app/config/` | olivecore, olivecodec, Qt::Core | **核心数据层**。节点图、时间线模型、Undo、配置。注意：当前 `Node.h` 包含部分 `render/` 头文件（缓存类型、作业枚举），需先进行**头文件解耦**（见 3.4）。 |
| `liboliverender.so` | `app/render/`（不含 OpenGL 具体后端） | olivenode, olivecodec, olivecore, Qt::Core, OpenColorIO | 渲染抽象层：`Renderer`, `RenderProcessor`, `RenderTicket`, `Job` 体系。 |
| `liboliveui.so` | `app/widget/`, `app/panel/`, `app/window/`, `app/dialog/`, `app/tool/`, `app/ui/` | olivenode, oliverender, olivecodec, olivecore, oliveaudio, Qt::Widgets, KDDockWidgets | **UI 层**。所有 Qt Widget 相关代码。 |
| `libolivetask.so` | `app/task/` | olivenode, olivecodec, oliverender, olivecore, Qt::Core | 任务调度系统。可独立成库，也可在初期并入 `liboliveui.so`。 |
| `olive-editor` | `main.cpp`, `core.cpp/h` | 上述全部 | 主可执行文件，仅保留入口和全局生命周期管理。 |

### 3.3 依赖关系图

```
                    ┌─────────────┐
                    │ olive-editor│
                    └──────┬──────┘
                           │ links all
    ┌──────────────────────┼──────────────────────┐
    │                      │                      │
    ▼                      ▼                      ▼
┌─────────┐        ┌─────────────┐        ┌─────────────┐
│liboliveui│        │libolivetask │        │liboliverender│
└────┬────┘        └──────┬──────┘        └──────┬──────┘
     │                    │                      │
     └────────────────────┼──────────────────────┘
                          │
                          ▼
                   ┌─────────────┐
                   │ libolivenode│
                   └──────┬──────┘
                          │
          ┌───────────────┼───────────────┐
          │               │               │
          ▼               ▼               ▼
   ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
   │liboliveaudio│ │libolivecodec│ │liboliveplugin│
   └──────┬──────┘ └──────┬──────┘ └─────────────┘
          │               │
          └───────────────┼───────────────┘
                          │
                          ▼
                   ┌─────────────┐
                   │libolivecore │
                   └─────────────┘
```

**依赖规则**：
- 禁止任何下层库依赖上层库。
- `libolivenode.so` 当前依赖 `render/` 的部分类型（`FrameHashCache`, `ShaderJob` 等枚举），需通过**前向声明（forward declare）**或**接口抽象**解耦。

### 3.4 接口与符号可见性

#### 3.4.1 导出宏定义

在每个模块的公共头目录（如 `app/node/api.h`）中定义：

```cpp
// app/node/api.h
#pragma once

#include "common/define.h"

#ifdef OLIVE_BUILDING_NODE
#  define OLIVE_NODE_API Q_DECL_EXPORT
#else
#  define OLIVE_NODE_API Q_DECL_IMPORT
#endif
```

所有需要跨库使用的类/函数均标记：

```cpp
// app/node/node.h
class OLIVE_NODE_API Node : public QObject { ... };
```

#### 3.4.2 Node 与 Render 的解耦

当前 `Node.h` 包含以下 render 头文件（经代码分析）：
- `render/rendercache.h`（`FrameHashCache` 等）
- `render/job/shaderjob.h` 等（作业类型）

**解耦策略**：

1. **枚举与前置声明**：将 `RenderTicket::ReturnType`, `RenderMode::Mode`, `PixelFormat` 等移到 `ext/core/` 或 `common/` 中，使其不依赖 `render/`。
2. **接口回调**：`Node` 中需要通知缓存失效的逻辑，改为通过 `NodeCacheInterface` 纯虚接口注入，而非直接引用 `FrameHashCache`。
3. **Job 类型**：`Node` 仅需要知道 `ShaderJob` 等类型的存在以支持虚函数分发，可以将 `ProcessShader` 等虚函数的参数从具体类型改为更抽象的 `const void *` 或基类指针，或把 `render/job/*.h` 中仅含数据定义的头文件移动到 `common/`。

### 3.5 CMake 改造要点

当前 `app/` 下的子模块通过修改 `PARENT_SCOPE` 变量 `OLIVE_SOURCES` 来汇报源文件，最终由 `app/CMakeLists.txt` 统一创建 `libolive-editor` OBJECT 库。改造后，每个子模块应自行产出库目标。

#### 3.5.1 子模块 CMakeLists.txt 改造示例

以 `app/node/CMakeLists.txt` 为例：

```cmake
# 改造前：仅收集源文件到 PARENT_SCOPE
# set(OLIVE_SOURCES ${OLIVE_SOURCES} node.cpp node.h PARENT_SCOPE)

# 改造后：创建本模块的 OBJECT/SHARED 库片段
add_subdirectory(project)
add_subdirectory(output)
# ... 其他子目录

set(NODE_SOURCES
  node.cpp
  node.h
  traverser.cpp
  traverser.h
  # ... 所有 node/ 下源文件
)

# 方案 A：本模块建 SHARED 库（推荐）
add_library(olivenode SHARED ${NODE_SOURCES})
target_link_libraries(olivenode
  PUBLIC
    olivecore
    olivecodec
    Qt${QT_VERSION_MAJOR}::Core
)
target_compile_definitions(olivenode PRIVATE OLIVE_BUILDING_NODE)
target_include_directories(olivenode
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/app>
    $<INSTALL_INTERFACE:include>
)

# 方案 B：本模块建 OBJECT 库，由上层组合为 SHARED 库
# add_library(olivenode-obj OBJECT ${NODE_SOURCES})
# ... 在 app/CMakeLists.txt 中组合
```

#### 3.5.2 顶层 app/CMakeLists.txt 改造

```cmake
# 各子模块自行创建库目标
add_subdirectory(audio)      # -> oliveaudio
add_subdirectory(codec)      # -> olivecodec
add_subdirectory(common)     # -> 并入 olivecodec 或单独 common
add_subdirectory(config)     # -> 并入 olivenode
add_subdirectory(node)       # -> olivenode
add_subdirectory(render)     # -> oliverender
add_subdirectory(task)       # -> olivetask
add_subdirectory(timeline)   # -> 并入 olivenode
add_subdirectory(undo)       # -> 并入 olivenode
add_subdirectory(widget)     # -> oliveui
add_subdirectory(panel)      # -> oliveui
add_subdirectory(window)     # -> oliveui
add_subdirectory(dialog)     # -> oliveui
add_subdirectory(tool)       # -> oliveui
add_subdirectory(ui)         # -> oliveui
add_subdirectory(pluginSupport) # -> oliveplugin

# 版本对象保持 OBJECT
add_library(olive-version-obj OBJECT version.cpp version.h)
target_link_libraries(olive-version-obj PRIVATE Qt${QT_VERSION_MAJOR}::Core)

# 主可执行文件
add_executable(olive-editor
  main.cpp
  core.cpp
  core.h
  $<TARGET_OBJECTS:olive-version-obj>
)

target_link_libraries(olive-editor PRIVATE
  oliveui
  olivetask
  oliverender
  olivenode
  oliveaudio
  olivecodec
  oliveplugin
  olivecore
  # ... 外部依赖
)
```

#### 3.5.3 平台注意事项

- **Windows**：`Q_DECL_EXPORT`/`Q_DECL_IMPORT` 会自动处理 `__declspec(dllexport/dllimport)`。需确保 `olive-editor.exe` 与所有 `.dll` 在同一目录，或通过 `PATH` 找到。
- **macOS**：动态库后缀为 `.dylib`。若打包为 `.app` Bundle，需使用 `install_name_tool` 或 CMake 的 `@rpath` 设置确保加载路径正确。
- **Linux**：使用 `RPATH` 或 `LD_LIBRARY_PATH`。打包时可用 `patchelf` 或 AppImage 工具。

---

## 4. 渲染器多进程化方案

### 4.1 进程模型

| 进程 | 职责 | 技术栈 |
|---|---|---|
| `olive-editor`（主进程） | GUI、项目数据管理、时间线编辑、用户交互 | Qt Widgets, KDDockWidgets |
| `olive-renderer`（子进程） | 节点图遍历、GPU/OpenGL 渲染、FFmpeg 解码、OFX 插件执行 | Qt Core（非 GUI）, OpenGL, FFmpeg, OCIO |

**启动方式**：主进程通过 `QProcess` 启动 `olive-renderer`，并捕获其 `stdin/stdout` 作为通信管道。子进程不使用任何 GUI 模块，仅初始化 `QCoreApplication` 和 OpenGL 离屏上下文。

### 4.2 IPC 通信协议（stdio）

#### 4.2.1 传输格式：NDJSON

采用 **Newline Delimited JSON（NDJSON）**，每行一条完整 JSON 消息，以 `\n` 分隔。理由：
- 基于文本，易于调试（`echo '{...}' | olive-renderer` 可手动测试）。
- 结构化，易于扩展新字段。
- 帧边界天然由换行符确定，无需额外的长度前缀或帧同步协议。

#### 4.2.2 消息定义

**请求消息（主进程 → 子进程，写入子进程 stdin）**：

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 消息类型：`init`, `render_frame`, `render_audio`, `cancel`, `shutdown`, `ping` |
| `req_id` | int | 请求唯一标识，用于响应匹配。 |
| `...` | 类型相关 | 见下表。 |

**响应消息（子进程 → 主进程，写入子进程 stdout）**：

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | string | 消息类型：`ready`, `result`, `error`, `cancelled`, `heartbeat`, `pong` |
| `req_id` | int | 对应请求的 `req_id`。对于主动推送（如 `heartbeat`），`req_id` 为 `0`。 |
| `...` | 类型相关 | 见下表。 |

#### 4.2.3 详细消息格式

```json
// === 初始化 ===
// 主进程 -> 子进程
{"type":"init","req_id":1,"backend":"opengl","shader_path":"/usr/share/olive/shaders","ocio_config_path":"/path/to/config.ocio"}

// 子进程 -> 主进程
{"type":"ready","req_id":1,"status":"ok","backend_version":"4.6 (Core Profile)"}
{"type":"error","req_id":1,"status":"error","message":"Failed to create OpenGL context"}

// === 渲染视频帧 ===
// 主进程 -> 子进程
{
  "type": "render_frame",
  "req_id": 2,
  "ticket_id": 101,
  "node_graph_ref": "a3f7b2d9",
  "time": "1001/30000",
  "video_params": {
    "width": 1920,
    "height": 1080,
    "depth": 1,
    "format": "rgba32f",
    "channel_count": 4,
    "pixel_aspect": "1/1"
  },
  "audio_params": {
    "sample_rate": 48000,
    "channel_layout": "stereo"
  },
  "mode": "offline",
  "color_manager": {
    "reference_space": "ACES - ACES2065-1",
    "display_space": "Rec.709"
  },
  "shm_name": "/olive_r_12345_2",
  "shm_size": 33177600
}

// 子进程 -> 主进程（成功）
{
  "type": "result",
  "req_id": 2,
  "ticket_id": 101,
  "status": "ok",
  "shm_name": "/olive_r_12345_2",
  "width": 1920,
  "height": 1080,
  "format": "rgba32f",
  "pixel_format_id": 28,
  "timestamp_ms": 45
}

// 子进程 -> 主进程（失败）
{
  "type": "error",
  "req_id": 2,
  "ticket_id": 101,
  "status": "error",
  "category": "decoder",
  "message": "Failed to decode frame at time 1001/30000: codec not found"
}

// === 渲染音频 ===
// 主进程 -> 子进程
{
  "type": "render_audio",
  "req_id": 3,
  "ticket_id": 102,
  "node_graph_ref": "a3f7b2d9",
  "range": {"in": "0/1", "out": "48000/48000"},
  "audio_params": {
    "sample_rate": 48000,
    "channel_layout": "stereo",
    "format": "flt_planar"
  },
  "mode": "offline",
  "shm_name": "/olive_r_12345_3",
  "shm_size": 384000
}

// === 取消任务 ===
{"type":"cancel","req_id":4,"ticket_id":101}
{"type":"cancelled","req_id":4,"ticket_id":101}

// === 心跳与探测 ===
{"type":"ping","req_id":5}
{"type":"pong","req_id":5}

// 子进程主动心跳（每 3 秒）
{"type":"heartbeat","req_id":0,"timestamp":1716288000}

// === 优雅退出 ===
{"type":"shutdown","req_id":6}
{"type":"result","req_id":6,"status":"ok"}
```

#### 4.2.4 通信时序示例

```
主进程                              子进程 (olive-renderer)
  │                                      │
  │── QProcess::start() ─────────────────>│
  │                                      │ 初始化 Qt Core
  │                                      │ 初始化 OpenGL 上下文
  │<── stdout: {"type":"ready",...} ─────│
  │                                      │
  │── stdin: render_frame (ticket #1) ──>│
  │── stdin: render_frame (ticket #2) ──>│  入队渲染
  │                                      │
  │<── stdout: result (ticket #1) ───────│  完成帧 #1
  │── 读取共享内存帧数据                   │
  │── shm_unlink()                       │
  │                                      │
  │<── stdout: result (ticket #2) ───────│  完成帧 #2
  │                                      │
  │── stdin: cancel (ticket #3) ────────>│  取消正在进行的 #3
  │<── stdout: cancelled (ticket #3) ────│
  │                                      │
  │── stdin: shutdown ──────────────────>│  清理资源，退出事件循环
  │<── stdout: result (shutdown) ────────│
  │── QProcess::waitForFinished() ──────>│  进程结束
```

### 4.3 数据平面：共享内存帧传输

NDJSON 仅适合传输控制命令和元数据。**视频帧（RGBA32F, 1920×1080 ≈ 33MB）和音频块**不能通过 base64 编码在 JSON 中传输（实时预览需要 24–30fps，stdio 带宽和 CPU 编解码开销均不可接受）。

#### 4.3.1 方案：POSIX / Windows 共享内存

**POSIX（Linux/macOS）**：

```cpp
// 主进程创建
int fd = shm_open("/olive_r_12345_2", O_RDWR | O_CREAT, 0666);
ftruncate(fd, shm_size);
void *ptr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

// 子进程打开（通过 shm_name 从 JSON 中读取）
int fd = shm_open("/olive_r_12345_2", O_RDWR, 0666);
void *ptr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

// 使用完毕后，主进程负责 unlink
shm_unlink("/olive_r_12345_2");
```

**Windows**：

```cpp
// 主进程创建
HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                0, shm_size, L"Local\\olive_r_12345_2");
void *ptr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, shm_size);

// 子进程打开
HANDLE hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, L"Local\\olive_r_12345_2");
void *ptr = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, shm_size);

// 清理
UnmapViewOfFile(ptr);
CloseHandle(hMap);
```

**帧数据布局**：共享内存前 256 字节保留为**元数据头**（Magic、版本、实际数据偏移、行间距 linesize、校验和），后续为原始像素/采样数据。

```
┌─────────────────────┬──────────────────────────────────────┐
│   Header (256 B)    │           Pixel Data                 │
│  magic | offset |   │  row 0  │  row 1  │ ... │  row H-1  │
│  linesize | ...     │  (width * channels * sizeof(float))  │
└─────────────────────┴──────────────────────────────────────┘
```

#### 4.3.2 备选方案：内存映射临时文件

若共享内存 API 在不同平台间行为不一致，可退而求其次使用**内存映射临时文件**：

```cpp
QTemporaryFile tmp;
tmp.setFileTemplate("olive_render_XXXXXX.raw");
tmp.open();
tmp.resize(shm_size);

// 内存映射
tmp.seek(0);
uchar *ptr = tmp.map(0, shm_size);

// 将文件路径（而非 shm_name）通过 JSON 传递
{"shm_path": "/tmp/olive_render_a1b2c3.raw", ...}
```

此方案兼容性最好，但性能略低于纯共享内存（因可能触发文件系统页缓存回写）。

### 4.4 节点图序列化与缓存

渲染请求的核心输入是**节点图（Node Graph）**。直接每次传输完整 XML 序列化在实时预览场景下（24–30fps）不可接受。

#### 4.4.1 策略：引用 + 增量更新

1. **首次传输**：当某个 `ViewerOutput` 需要渲染时，主进程将其关联的节点图通过 `type: init_graph` 消息完整序列化发送给子进程，子进程缓存并返回一个 `graph_ref`（如 SHA-256 前 8 位）。

   ```json
   {"type":"init_graph","req_id":10,"graph_ref":"a3f7b2d9","node_graph_xml":"...<Project>...</Project>..."}
   {"type":"result","req_id":10,"graph_ref":"a3f7b2d9","node_count":42}
   ```

2. **后续引用**：渲染帧请求通过 `"node_graph_ref": "a3f7b2d9"` 引用已缓存的图，无需重复传输 XML。

3. **增量更新**：当用户调整某个节点的参数时，主进程发送 `update_graph` 消息，仅携带变更的节点 ID 和参数字段。

   ```json
   {"type":"update_graph","req_id":11,"graph_ref":"a3f7b2d9","updates":[{"node_id":"Transform1","params":{"position":{"x":100,"y":200}}}]}
   ```

4. **序列化复用**：直接复用现有的 `ProjectSerializer`，以 `kOnlyNodes` 模式序列化目标 `ViewerOutput` 及其上游依赖节点。

#### 4.4.2 子进程中的节点图重建

子进程收到 `init_graph` 后：
1. 使用 `ProjectSerializer::Load()` 将 XML 反序列化为临时 `Project` 对象。
2. 提取目标 `ViewerOutput` 节点，构建本地 `NodeValueDatabase`。
3. 将图对象存入 `graph_ref → Project` 的映射表中。
4. 后续 `render_frame` 直接使用缓存的图，避免重复解析 XML。

### 4.5 渲染进程生命周期管理

#### 4.5.1 启动与就绪

```cpp
// RenderManager::CreateInstance()
render_process_ = new QProcess(this);
render_process_->setProgram(QCoreApplication::applicationDirPath() + "/olive-renderer");
render_process_->setArguments({"--backend", "opengl"});
render_process_->start();

// 等待 ready 消息（带超时）
connect(render_process_, &QProcess::readyReadStandardOutput, this, &RenderManager::ReadStdout);
```

#### 4.5.2 心跳与卡死检测

- 子进程每 **3 秒**主动输出 `heartbeat`。
- 主进程每 **5 秒**发送 `ping`，若 **10 秒**内未收到 `pong`，认为子进程卡死。
- 卡死处理：
  1. `render_process_->kill()` 强制终止。
  2. 清理所有未完成的 `RenderTicket`，标记为错误状态。
  3. 自动重启子进程。
  4. 重新发送所有活跃的 `graph_ref` 对应的节点图。

#### 4.5.3 崩溃恢复

```cpp
connect(render_process_, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred),
        this, [this](QProcess::ProcessError error) {
    if (error == QProcess::Crashed) {
        qWarning() << "Renderer process crashed. Restarting...";
        RestartRendererProcess();
        // 通知 UI 显示"渲染器已崩溃并恢复"的提示
    }
});
```

**关键收益**：即使 OpenGL 驱动崩溃、OFX 插件 segfault、FFmpeg 解码器触发未处理异常，也只会导致 `olive-renderer` 子进程终止，主进程的 GUI、项目数据、Undo 栈均完好无损。

#### 4.5.4 优雅退出

主进程析构时：
1. 发送 `shutdown` 请求，等待子进程返回 `result`（超时 5 秒）。
2. 若子进程未退出，调用 `terminate()`，再等 3 秒。
3. 若仍未退出，`kill()` 强制结束。

### 4.6 主进程 RenderManager 适配

当前 `RenderManager` 直接创建并管理 `RenderThread`。改造后，`RenderManager` 转型为 **IPC 客户端管理器**，对外接口保持**完全不变**，以最小化 UI 层侵入。

#### 4.6.1 类结构调整

```cpp
// 新增：轻量级 IPC 客户端
class RenderServiceClient : public QObject {
    Q_OBJECT
public:
    explicit RenderServiceClient(QObject *parent = nullptr);
    bool Start(const QString &renderer_executable);
    void Shutdown();

    // 异步发送渲染请求，返回内部 ticket_id
    int RequestRenderFrame(const RenderManager::RenderVideoParams &params, const QString &graph_ref);
    int RequestRenderAudio(const RenderManager::RenderAudioParams &params, const QString &graph_ref);
    void CancelTicket(int ticket_id);

signals:
    void ResultReceived(int ticket_id, const QJsonObject &result);
    void ErrorReceived(int ticket_id, const QString &message);
    void ProcessCrashed();
    void ProcessRecovered();

private:
    QProcess *process_;
    QHash<int, RenderTicketPtr> ticket_map_;  // ticket_id -> RenderTicket
    // ...
};

// 改造后的 RenderManager（对外接口不变）
class RenderManager : public QObject {
    Q_OBJECT
public:
    // 原有接口 100% 保留
    RenderTicketPtr RenderFrame(const RenderVideoParams &params);
    RenderTicketPtr RenderAudio(const RenderAudioParams &params);
    bool RemoveTicket(RenderTicketPtr ticket);

private:
    // 旧实现：RenderThread *video_thread_; ...
    // 新实现：
    RenderServiceClient *client_;
    QHash<Node*, QString> node_graph_refs_;  // ViewerOutput -> graph_ref
};
```

#### 4.6.2 渲染流程映射

```
UI/Widget 层
    │ RenderManager::RenderFrame(params)
    │
    ▼
RenderManager
    │ 1. 检查 params.node 对应的 graph_ref 是否已在子进程缓存
    │ 2. 若未缓存，序列化节点图 -> init_graph -> 获取 graph_ref
    │ 3. 创建共享内存 -> shm_name
    │ 4. 将 params + graph_ref + shm_name 打包为 NDJSON
    │
    ▼
RenderServiceClient -> QProcess::write() -> 子进程 stdin
    │
    │<── stdout: NDJSON result
    ▼
RenderManager 从共享内存读取帧数据
    │ 构造 FramePtr/TexturePtr
    │ 调用 RenderTicket::Finish(result)
    ▼
UI 层收到 RenderTicketWatcher::Finished 信号，更新显示
```

#### 4.6.3 兼容性保留

- 保留 `RenderTicket`, `RenderTicketWatcher`, `RenderTicketPtr` 的完整语义。
- 保留 `PreviewAutoCacher` 的接口，其内部调用 `RenderManager` 的方式无需修改。
- 保留 `RenderMode::Mode`, `ReturnType` 等枚举定义位置，或在 `ext/core/` 中建立同义定义。

---

## 5. 实施路线图

### 5.1 第一阶段：动态库基础拆分（预估 2–3 周）

**目标**：完成低耦合模块的动态库化，建立符号导出规范和 CMake 新范式。

| 任务 | 说明 |
|---|---|
| T1.1 | 将 `ext/core/` 的构建类型由 `STATIC` 改为 `SHARED`，验证所有平台加载正常。 |
| T1.2 | 创建 `libolivecodec.so`：合并 `app/codec/` + `app/common/`，处理跨平台符号导出。 |
| T1.3 | 创建 `liboliveplugin.so`：将 `pluginSupport/` + `OfxHost` 独立，验证 OFX 插件加载。 |
| T1.4 | 创建 `liboliveaudio.so`：将 `app/audio/` 独立。 |
| T1.5 | 引入 `OLIVE_API` 宏体系，为每个模块定义导出/导入宏。 |
| T1.6 | 在 CI 中增加动态库加载路径测试，确保 `olive-editor` 能在干净环境中启动。 |

**里程碑**：`olive-editor` 可正常启动，所有原有功能不变，但内部已由 1 个 OBJECT 库变为 4+ 个动态库。

### 5.2 第二阶段：核心层拆分与渲染进程化（预估 4–6 周）

**目标**：完成 `node/` 与 `render/` 的解耦，并实现 `olive-renderer` 子进程。

| 任务 | 说明 |
|---|---|
| T2.1 | **Node/Render 解耦**：将 `Node.h` 中对 `render/` 的包含移除，迁移依赖类型到 `common/` 或 `ext/core/`；将 `FrameHashCache` 交互改为接口注入。 |
| T2.2 | 创建 `libolivenode.so`：包含 `node/`, `timeline/`, `undo/`, `config/`。 |
| T2.3 | 创建 `liboliverender.so`：包含 `render/`（不含 OpenGL 后端具体平台代码），依赖 `olivenode` + `olivecodec`。 |
| T2.4 | 创建 `olive-renderer` 可执行文件目标，复用 `liboliverender.so` + `libolivenode.so` + `libolivecodec.so` + `libolivecore.so`。 |
| T2.5 | 实现 **NDJSON IPC 协议**：在子进程中实现 `RenderService`（基于 `QSocketNotifier` 监听 `stdin`）；在主进程中实现 `RenderServiceClient`。 |
| T2.6 | 实现 **共享内存帧传输**：封装 `SharedMemoryBuffer` 类，支持 POSIX + Windows API，统一为 `Create/Attach/Detach/Destroy` 接口。 |
| T2.7 | 实现 **节点图序列化缓存**：在 `RenderServiceClient` 中维护 `graph_ref` 映射表；在子进程中维护反序列化后的节点图缓存。 |
| T2.8 | 改造 `RenderManager`：将内部 `RenderThread` 调度替换为 `RenderServiceClient` IPC 调用，对外接口保持不变。 |
| T2.9 | 实现 **崩溃恢复与心跳**：子进程卡死/崩溃检测，自动重启，重新同步节点图缓存。 |
| T2.10 | 全面回归测试：预览、导出、音频回放、OFX 插件、色彩管理。 |

**里程碑**：渲染在子进程中稳定运行，手动触发子进程崩溃（如 `kill -9`）后，主进程可自动恢复且 GUI 不闪退。

### 5.3 第三阶段：优化与稳定化（预估 2–3 周）

| 任务 | 说明 |
|---|---|
| T3.1 | **增量节点图更新**：实现 `update_graph` 消息，避免参数微调时重复传输完整 XML。 |
| T3.2 | **多渲染进程**：支持同时启动多个 `olive-renderer` 进程（如一个用于预览，一个用于后台导出），提升并行度。 |
| T3.3 | **性能基准测试**：对比单进程 vs 多进程的帧渲染延迟、内存占用、CPU 开销，优化共享内存拷贝次数。 |
| T3.4 | **打包适配**：更新 macOS `.app` Bundle、Windows Installer、Linux AppImage 的打包脚本，确保动态库和 `olive-renderer` 被正确包含。 |
| T3.5 | 文档更新：更新 `build.md`、`build-macos-zh.md`，说明新的运行时依赖和动态库加载路径配置。 |

---

## 6. 风险与对策

| 风险 | 影响 | 对策 |
|---|---|---|
| **Node/Render 解耦工作量超预期** | 高 | 采用"接口抽象 + 前向声明"的轻量解耦，不追求完全消除逻辑耦合，只消除编译期头文件依赖。若实在无法解耦，可将 `node/` + `render/` 暂时合并为一个 `libolive-engine.so`，后续再拆分。 |
| **共享内存跨平台兼容性** | 中 | 封装抽象层 `SharedMemoryBuffer`，POSIX 和 Windows 分别实现。若某平台支持不佳，自动降级为内存映射临时文件。 |
| **NDJSON 协议性能瓶颈** | 中 | 控制消息数据量极小（<1KB），不会是瓶颈。若未来需要更高吞吐，可无损升级为 **MessagePack**（二进制 JSON 兼容格式），无需改协议语义。 |
| **子进程启动延迟影响首帧** | 中 | 采用**预启动策略**：主进程启动后立即在后台启动 `olive-renderer`，用户打开项目时渲染器已就绪。 |
| **GPU/OpenGL 上下文跨进程问题** | 中 | 子进程独立创建离屏 OpenGL 上下文（`QOffscreenSurface`），主进程不再直接操作 GL。主进程 UI 显示通过共享内存获取 CPU 帧数据，或使用平台特定的 GL 共享纹理（进阶优化，初期不做）。 |
| **OFX 插件在子进程中的稳定性** | 高 | 这正是多进程架构的收益点。OFX 插件崩溃仅影响子进程，主进程安全。需确保 OFX 插件资源路径通过 `--resource-path` 参数传递给子进程。 |
| **Qt 信号/槽跨动态库** | 低 | Qt 的元对象系统原生支持跨动态库，只要确保 `moc` 编译了含 `Q_OBJECT` 宏的公共头文件，且动态库被正确链接。 |

---

## 7. 附录：完整消息协议定义

### 7.1 请求消息（主进程 → 子进程）

```typescript
// 基础请求结构
interface Request {
  type: string;
  req_id: number; // >0
}

interface InitRequest extends Request {
  type: "init";
  backend: "opengl" | "dummy";
  shader_path: string;
  ocio_config_path?: string;
}

interface InitGraphRequest extends Request {
  type: "init_graph";
  graph_ref: string;        // 主进程生成的图引用 ID
  node_graph_xml: string;   // 完整的项目 XML（kOnlyNodes 模式）
}

interface UpdateGraphRequest extends Request {
  type: "update_graph";
  graph_ref: string;
  updates: Array<{
    node_id: string;
    params: Record<string, any>;
  }>;
}

interface RenderFrameRequest extends Request {
  type: "render_frame";
  ticket_id: number;
  graph_ref: string;
  time: string;             // 有理数字符串，如 "1001/30000"
  video_params: VideoParams;
  audio_params: AudioParams;
  mode: "offline" | "online";
  color_manager?: ColorManagerInfo;
  force_size?: { width: number; height: number };
  force_format?: string;
  shm_name: string;
  shm_size: number;
}

interface RenderAudioRequest extends Request {
  type: "render_audio";
  ticket_id: number;
  graph_ref: string;
  range: { in: string; out: string };
  audio_params: AudioParams;
  mode: "offline" | "online";
  generate_waveforms: boolean;
  shm_name: string;
  shm_size: number;
}

interface CancelRequest extends Request {
  type: "cancel";
  ticket_id: number;
}

interface ShutdownRequest extends Request {
  type: "shutdown";
}

interface PingRequest extends Request {
  type: "ping";
}
```

### 7.2 响应消息（子进程 → 主进程）

```typescript
// 基础响应结构
interface Response {
  type: string;
  req_id: number; // 对应请求，0 表示主动推送
}

interface ReadyResponse extends Response {
  type: "ready";
  status: "ok" | "error";
  backend_version?: string;
  message?: string; // 当 status=error 时
}

interface ResultResponse extends Response {
  type: "result";
  ticket_id?: number;
  status: "ok";
  // 对于渲染结果
  shm_name?: string;
  width?: number;
  height?: number;
  format?: string;
  pixel_format_id?: number;
  timestamp_ms?: number; // 渲染耗时
}

interface ErrorResponse extends Response {
  type: "error";
  ticket_id?: number;
  status: "error";
  category?: "decoder" | "shader" | "plugin" | "system" | "unknown";
  message: string;
}

interface CancelledResponse extends Response {
  type: "cancelled";
  ticket_id: number;
}

interface PongResponse extends Response {
  type: "pong";
}

interface HeartbeatResponse extends Response {
  type: "heartbeat";
  timestamp: number; // Unix timestamp (seconds)
}
```

### 7.3 共享内存布局（二进制）

```c
#define OLIVE_SHM_MAGIC 0x4F4C4956  // 'OLIV'
#define OLIVE_SHM_VERSION 1

struct ShmHeader {
    uint32_t magic;           // OLIVE_SHM_MAGIC
    uint32_t version;         // OLIVE_SHM_VERSION
    uint32_t data_offset;     // 像素/采样数据起始偏移（通常 256）
    uint32_t width;           // 帧宽（视频）或采样数（音频）
    uint32_t height;          // 帧高
    uint32_t depth;           // 3D 纹理深度
    uint32_t channel_count;   // 通道数
    uint32_t pixel_format;    // PixelFormat 枚举值
    uint32_t linesize;        // 每行字节数（可能包含 padding）
    uint64_t data_size;       // 实际数据字节数
    uint64_t checksum;        // CRC64（可选校验）
    uint8_t  reserved[256 - 48]; // 填充至 256 字节
};
// 紧接着 ShmHeader 之后为原始数据
```

---

## 8. 结论

本方案通过**动态库分层拆分**将 Olive/Oak 从单体编译单元演进为模块化架构，显著提升编译效率和代码边界清晰度；通过**渲染器多进程化**将最易崩溃的 GPU/OFX/FFmpeg 逻辑隔离到独立进程，利用 **stdio + NDJSON + 共享内存** 实现低延迟 IPC，从根本上解决"渲染崩溃导致编辑器闪退丢工作"的痛点。

实施上采用**渐进式路线**：先拆分外围低耦合模块建立规范，再攻克核心 Node/Render 解耦与进程化。对外接口（`RenderManager::RenderFrame` 等）保持完全兼容，UI 层无需感知底层架构变化。
