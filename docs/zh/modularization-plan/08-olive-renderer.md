# olive-renderer — 多进程渲染可执行文件

> **类型**：独立可执行文件（非动态库）  
> **依赖**：`libolivecore.so`, `libolivecodec.so`, `libolivenode.so`, `liboliverender.so`（可静态或动态链接）  
> **外部依赖**：Qt::Core, OpenGL, FFmpeg, OpenColorIO  
> **核心特征**：**"用完即弃"**——每帧（或每几帧）启动一个新进程，渲染完成后立即退出  
> **改造难度**：⭐⭐⭐（中等，逻辑复杂但隔离清晰）

---

## 1. 设计哲学：用完即弃（Fire-and-Forget）

传统多进程渲染架构通常维护一个**常驻子进程**，通过复杂的 IPC 协议进行状态同步、心跳检测、崩溃恢复。这种架构虽然成熟，但引入了以下复杂性：

- **状态同步**：主进程和子进程的节点图、缓存、参数必须保持一致。
- **锁与并发**：共享内存的读写需要锁或原子操作。
- **崩溃恢复**：子进程崩溃后需要重建状态、重同步节点图。
- **生命周期管理**：启动、握手、心跳、优雅退出、强制杀死的完整状态机。

本方案采用**激进的简化策略**：

> **每一帧渲染任务 = 一个全新的操作系统进程**。进程接收完整的渲染输入，执行渲染，输出结果，然后 `exit(0)`。

### 1.1 优势

| 优势 | 说明 |
|---|---|
| **零锁** | 进程之间不共享任何可变状态（除了只读的共享内存输出区），无需任何互斥锁、信号量、条件变量。 |
| **零状态同步** | 每帧的输入都是自包含的（节点图 XML + 渲染参数），子进程无需维护任何跨帧状态。 |
| **自动崩溃隔离** | 某一帧的渲染崩溃（OFX 插件 segfault、GPU 驱动错误）只会影响该进程，主进程和其他帧完全不受影响。 |
| **资源自动回收** | 进程退出后，操作系统自动回收其所有资源（内存、GPU 上下文、文件句柄、解码器实例），无需显式清理。 |
| **可预测性** | 没有状态泄漏、没有内存碎片累积、没有僵尸缓存，每一帧都在干净的环境中渲染。 |
| **易于调试** | 可以单独运行 `olive-renderer` 命令行重放某一帧的渲染，无需启动完整 GUI。 |

### 1.2 挑战与回退

| 挑战 | 分析 | 回退策略 |
|---|---|---|
| **进程启动开销** | `QProcess::start()` + OpenGL 上下文初始化可能需要 50–200ms | 若实测开销过高，采用 **"批处理模式"**：一个进程渲染 N 帧（如 5 帧），然后退出。或预启动一个进程池，但每个进程仍只服务一个批次后自杀。 |
| **节点图序列化开销** | 每帧都序列化完整节点图可能耗时 | 节点图 XML 在主进程缓存，相同图只序列化一次；仅参数变化时发送增量更新（即使进程用完即弃，输入数据仍可复用）。 |
| **GPU 上下文反复创建** | OpenGL 上下文创建/销毁开销较大 | 采用 **批处理模式** 摊销开销；或在支持的平台使用 EGL/GLES 的轻量上下文。 |
| **共享内存创建开销** | 每帧创建新的 shm 对象 | 使用 **内存映射临时文件** 替代 POSIX shm，创建开销更低；或主进程预分配一组循环缓冲区。 |
| **音频连续性** | 音频需要连续播放，逐帧进程可能导致间隙 | 音频采用 **批处理模式**：一个进程渲染 0.5–1 秒的音频块，而非每帧一个进程。 |

---

## 2. 进程模型详解

### 2.1 单帧模式（默认）

```
主进程（UI 线程或工作线程）
    │
    │ 1. 序列化节点图（若未缓存则生成 XML）
    │ 2. 创建共享内存 / 临时文件
    │ 3. 组装渲染参数
    │
    ▼
┌─────────────────────────────────────┐
│ QProcess::start("olive-renderer",   │
│   ["--mode=frame",                  │
│    "--node-graph=/tmp/g_123.xml",   │
│    "--time=1001/30000",             │
│    "--output-shm=/olive_r_123"])    │
└─────────────────────────────────────┘
    │
    │ 4. 等待子进程结束（阻塞或异步）
    │    QProcess::waitForFinished(timeout_ms)
    │
    ▼
子进程启动 ──────────────────────────────► 子进程退出
    │                                          │
    │ a. 解析命令行参数                          │
    │ b. 加载节点图 XML                          │
    │ c. 初始化 Qt Core + OpenGL                 │
    │ d. 执行 RenderProcessor                    │
    │ e. 将帧写入共享内存                          │
    │ f. 输出结果 JSON 到 stdout                   │
    │ g. exit(0)                                 │
    ▼                                          ▼
主进程读取共享内存 ──────────────────────► 主进程释放共享内存
    │
    │ 5. 将帧数据上传到 GPU Texture 或显示
    ▼
ViewerWidget 更新
```

### 2.2 批处理模式（性能回退）

当实测单帧模式开销过高时，启用批处理：

```
主进程
    │
    │ 渲染帧 #1, #2, #3, #4, #5
    ▼
启动 olive-renderer
    --mode=batch
    --frames=5
    --times=0/30,1/30,2/30,3/30,4/30
    --output-shm=/olive_r_batch_1
    --output-shm-size=165888000  (5 * 33MB)
    │
    ▼
子进程依次渲染 5 帧，全部写入同一块共享内存的不同偏移
输出 JSON 数组包含 5 个结果
exit(0)
```

---

## 3. 命令行接口

### 3.1 参数定义

```
olive-renderer [选项]

全局选项：
  --backend=<backend>        渲染后端：opengl（默认）, dummy
  --shader-path=<path>       着色器资源目录（默认：可执行文件同级目录下的 shaders/）
  --ocio-config=<path>       OpenColorIO 配置文件路径
  --verbose                  输出详细日志到 stderr

单帧模式（--mode=frame）：
  --mode=frame
  --node-graph=<path>        节点图 XML 文件路径
  --output-node=<id>         输出节点 ID（默认：图中的第一个 ViewerOutput）
  --time=<rational>          渲染时间点，如 "1001/30000"
  --video-params=<json>      视频参数 JSON，如 '{"width":1920,"height":1080,"format":"rgba32f"}'
  --audio-params=<json>      音频参数 JSON
  --color-ref=<space>        参考色彩空间
  --color-display=<space>    显示色彩空间
  --output-shm=<name>        POSIX 共享内存名称（如 "/olive_r_123"）或临时文件路径
  --output-shm-size=<bytes>  共享内存大小
  --output-stdout            将帧数据 base64 编码输出到 stdout（仅小帧/测试用）

批处理模式（--mode=batch）：
  --mode=batch
  --node-graph=<path>
  --output-node=<id>
  --times=<csv>              逗号分隔的时间点列表，如 "0/30,1/30,2/30"
  --video-params=<json>
  --audio-params=<json>
  --output-shm=<name>
  --output-shm-size=<bytes>

音频模式（--mode=audio）：
  --mode=audio
  --node-graph=<path>
  --output-node=<id>
  --start=<rational>         起始时间
  --duration=<rational>      持续时间
  --audio-params=<json>
  --output-shm=<name>
  --output-shm-size=<bytes>
```

### 3.2 使用示例

```bash
# 单帧渲染
olive-renderer \
  --mode=frame \
  --node-graph=/tmp/project_graph.xml \
  --output-node=ViewerOutput1 \
  --time=1001/30000 \
  --video-params='{"width":1920,"height":1080,"format":"rgba32f","channel_count":4}' \
  --output-shm=/olive_frame_12345 \
  --output-shm-size=33177600

# 批处理渲染 5 帧
olive-renderer \
  --mode=batch \
  --node-graph=/tmp/project_graph.xml \
  --times="0/24,1/24,2/24,3/24,4/24" \
  --output-shm=/olive_batch_67890 \
  --output-shm-size=165888000
```

---

## 4. 输出格式

### 4.1 标准输出（stdout）

子进程将渲染结果以 **单行 JSON** 输出到 stdout，然后退出。

**单帧成功**：
```json
{"status":"ok","mode":"frame","time":"1001/30000","width":1920,"height":1080,"format":"rgba32f","pixel_format_id":28,"shm_name":"/olive_frame_12345","data_offset":256,"data_size":33177600,"linesize":7680,"render_time_ms":42}
```

**批处理成功**：
```json
{"status":"ok","mode":"batch","frame_count":5,"frames":[{"time":"0/24","data_offset":256,"data_size":33177600},{"time":"1/24","data_offset":33178056,"data_size":33177600},...],"render_time_ms":180}
```

**错误**：
```json
{"status":"error","error_code":"decoder_failure","message":"Failed to open decoder for footage 'clip001.mp4': codec not found","time":"1001/30000"}
```

**被取消**（主进程 kill 时不会收到，因为进程已死；但对于批处理中的内部取消）：
```json
{"status":"cancelled","frames_completed":3,"frames_total":5}
```

### 4.2 标准错误（stderr）

- `--verbose` 模式下，详细的调试日志输出到 stderr。
- 错误信息同时出现在 stderr（人类可读）和 stdout JSON（机器可读）中。

### 4.3 退出码

| 退出码 | 含义 |
|---|---|
| 0 | 渲染成功 |
| 1 | 通用错误 |
| 2 | 无效参数 |
| 3 | 初始化失败（OpenGL/OCIO 等） |
| 4 | 节点图加载失败 |
| 5 | 渲染过程中出错（解码失败、着色器编译失败等） |
| 6 | 输出写入失败（共享内存不足等） |
| 130 | 被信号中断（SIGINT，即主进程 kill） |
| 137 | 被 SIGKILL 终止 |

---

## 5. 内部架构

```
olive-renderer (main.cpp)
    │
    ▼
┌─────────────────────────────┐
│ 1. 解析命令行参数            │
│    (QCommandLineParser)      │
└─────────────┬───────────────┘
              │
┌─────────────▼───────────────┐
│ 2. 初始化 QCoreApplication   │
│    (无 GUI，无事件循环)       │
└─────────────┬───────────────┘
              │
┌─────────────▼───────────────┐
│ 3. 加载节点图 XML            │
│    ProjectSerializer::Load() │
│    找到目标 ViewerOutput     │
└─────────────┬───────────────┘
              │
┌─────────────▼───────────────┐
│ 4. 初始化渲染后端            │
│    OpenGL: QOffscreenSurface │
│            + QOpenGLContext  │
│    Dummy: 空实现             │
└─────────────┬───────────────┘
              │
┌─────────────▼───────────────┐
│ 5. 执行渲染                  │
│    RenderProcessor::Process()│
│    遍历节点图 → 生成帧       │
└─────────────┬───────────────┘
              │
┌─────────────▼───────────────┐
│ 6. 写入共享内存              │
│    mmap/shm_open/MapViewOfFile│
│    写入 ShmHeader + 像素数据  │
└─────────────┬───────────────┘
              │
┌─────────────▼───────────────┐
│ 7. 输出 JSON 结果到 stdout   │
│ 8. 清理（可选，因即将 exit）  │
│ 9. return status;            │
└─────────────────────────────┘
```

### 5.1 主函数伪代码

```cpp
// app/render/renderer_main.cpp

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include "olive/render_api.h"
#include "olive/node_api.h"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;

    // 定义命令行选项
    parser.addOption({"mode", "Render mode: frame, batch, audio", "mode", "frame"});
    parser.addOption({"node-graph", "Node graph XML file", "path"});
    parser.addOption({"output-node", "Output node ID", "id"});
    parser.addOption({"time", "Frame time (rational)", "time"});
    parser.addOption({"times", "Batch frame times (csv)", "csv"});
    parser.addOption({"video-params", "Video params JSON", "json"});
    parser.addOption({"audio-params", "Audio params JSON", "json"});
    parser.addOption({"output-shm", "Output shared memory name", "name"});
    parser.addOption({"output-shm-size", "Output shared memory size", "bytes"});
    parser.addOption({"backend", "Render backend", "backend", "opengl"});
    parser.addOption({"shader-path", "Shader directory path", "path"});
    parser.addOption({"verbose", "Verbose logging"});

    parser.process(app);

    // 验证必需参数
    if (!parser.isSet("node-graph") || !parser.isSet("output-shm")) {
        OutputError("Missing required arguments: --node-graph and --output-shm");
        return 2;
    }

    // 1. 加载节点图
    OliveNodeGraph* graph = olive_node_graph_create();
    QFile xml_file(parser.value("node-graph"));
    if (!xml_file.open(QIODevice::ReadOnly)) {
        OutputError("Failed to open node graph file");
        return 4;
    }
    QByteArray xml_data = xml_file.readAll();
    if (olive_node_graph_load_xml(graph, xml_data.constData(), xml_data.size()) != OLIVE_OK) {
        OutputError("Failed to parse node graph XML");
        return 4;
    }

    // 2. 找到输出节点
    const char* output_node_id = parser.value("output-node").toUtf8().constData();
    OliveNode* output_node = olive_node_graph_find_node(graph, output_node_id);
    if (!output_node) {
        // 如果没指定，找第一个 ViewerOutput
        // ...
    }

    // 3. 初始化渲染上下文
    OliveRenderBackend backend = parser.value("backend") == "dummy"
                                     ? OLIVE_RENDER_BACKEND_DUMMY
                                     : OLIVE_RENDER_BACKEND_OPENGL;
    OliveRenderContext* ctx = olive_render_context_create(backend);
    if (!ctx) {
        OutputError("Failed to create render context");
        return 3;
    }
    if (olive_render_context_init(ctx) != OLIVE_OK) {
        OutputError("Failed to initialize render backend");
        return 3;
    }

    // 4. 准备共享内存
    QString shm_name = parser.value("output-shm");
    size_t shm_size = parser.value("output-shm-size").toULongLong();
    void* shm_ptr = MapSharedMemory(shm_name, shm_size);
    if (!shm_ptr) {
        OutputError("Failed to map shared memory");
        return 6;
    }

    // 5. 执行渲染
    QElapsedTimer timer;
    timer.start();

    QString mode = parser.value("mode");
    QJsonObject result;

    if (mode == "frame") {
        // 单帧渲染
        OliveRenderFrameParams params = ParseFrameParams(parser, graph, output_node);
        void* frame_data = nullptr;
        size_t frame_size = 0;
        int w, h;
        OlivePixelFormat fmt;
        int err = olive_render_frame_sync(ctx, &params, &frame_data, &frame_size, &w, &h, &fmt);

        if (err == OLIVE_OK) {
            // 写入共享内存
            WriteFrameToShm(shm_ptr, frame_data, frame_size, w, h, fmt);
            olive_core_free(frame_data);

            result["status"] = "ok";
            result["mode"] = "frame";
            result["width"] = w;
            result["height"] = h;
            result["shm_name"] = shm_name;
            result["data_offset"] = 256;
            result["data_size"] = static_cast<qint64>(frame_size);
        } else {
            result["status"] = "error";
            result["message"] = olive_core_last_error_string();
        }
    } else if (mode == "batch") {
        // 批处理渲染...
    }

    result["render_time_ms"] = timer.elapsed();

    // 6. 输出 JSON
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << std::endl;

    // 7. 清理
    UnmapSharedMemory(shm_ptr, shm_size);
    olive_render_context_destroy(ctx);
    olive_node_graph_destroy(graph);

    return result["status"].toString() == "ok" ? 0 : 5;
}
```

---

## 6. 共享内存实现

### 6.1 跨平台封装

```cpp
// app/render/shared_memory.h

#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* olive_shm_create(const char* name, size_t size);
void* olive_shm_open(const char* name, size_t size);
void olive_shm_close(void* ptr, size_t size);
void olive_shm_unlink(const char* name);

#ifdef __cplusplus
}
#endif
```

### 6.2 POSIX 实现

```cpp
#include "shared_memory.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

void* olive_shm_create(const char* name, size_t size) {
    int fd = shm_open(name, O_RDWR | O_CREAT, 0666);
    if (fd < 0) return nullptr;
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return nullptr;
    }
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
}

void* olive_shm_open(const char* name, size_t size) {
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd < 0) return nullptr;
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return ptr;
}

void olive_shm_close(void* ptr, size_t size) {
    if (ptr) munmap(ptr, size);
}

void olive_shm_unlink(const char* name) {
    shm_unlink(name);
}
```

### 6.3 Windows 实现

```cpp
#include <windows.h>

void* olive_shm_create(const char* name, size_t size) {
    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
                                      PAGE_READWRITE, 0, static_cast<DWORD>(size), name);
    if (!hMap) return nullptr;
    return MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, size);
    // 注意：句柄需要保存以便后续关闭，此处简化
}

void olive_shm_close(void* ptr, size_t size) {
    (void)size;
    if (ptr) UnmapViewOfFile(ptr);
}
```

---

## 7. 主进程中的集成

### 7.1 渲染一帧的封装

```cpp
// app/render/render_process_launcher.h

#pragma once
#include <QString>
#include <QProcess>
#include <QJsonObject>
#include "olive/core_api.h"
#include "olive/node_api.h"

namespace olive {

class RenderProcessLauncher {
public:
    struct FrameResult {
        bool success = false;
        QString error_message;
        int width = 0;
        int height = 0;
        OlivePixelFormat format = OLIVE_PIXEL_FMT_INVALID;
        QString shm_name;
        size_t data_offset = 0;
        size_t data_size = 0;
        int render_time_ms = 0;
    };

    // 渲染单帧，阻塞直到子进程结束
    static FrameResult RenderFrameSync(const QString& nodeGraphXmlPath,
                                        const QString& outputNodeId,
                                        OliveRational time,
                                        const OliveVideoParams& videoParams,
                                        const OliveAudioParams& audioParams,
                                        OliveRenderMode mode,
                                        int timeoutMs = 30000);

    // 渲染单帧，异步（返回 QProcess*，调用方连接 finished 信号）
    static QProcess* RenderFrameAsync(const QString& nodeGraphXmlPath,
                                       const QString& outputNodeId,
                                       OliveRational time,
                                       const OliveVideoParams& videoParams,
                                       const OliveAudioParams& audioParams,
                                       OliveRenderMode mode);

private:
    static QString BuildShmName();
    static bool CreateShm(const QString& name, size_t size, void** outPtr);
    static void DestroyShm(const QString& name, void* ptr, size_t size);
};

}  // namespace olive
```

### 7.2 使用示例

```cpp
// ViewerWidget 中请求渲染当前帧
void ViewerWidget::RequestFrameAtTime(OliveRational time) {
    // 1. 确保节点图 XML 已缓存
    if (cached_graph_xml_path_.isEmpty()) {
        cached_graph_xml_path_ = SerializeNodeGraphToTempFile(viewer_output_);
    }

    // 2. 创建共享内存
    QString shm_name = RenderProcessLauncher::BuildShmName();
    size_t shm_size = CalculateFrameSize(video_params_);

    // 3. 启动子进程（异步）
    QProcess* proc = RenderProcessLauncher::RenderFrameAsync(
        cached_graph_xml_path_,
        olive_node_get_id(reinterpret_cast<OliveNode*>(viewer_output_)),
        time,
        video_params_,
        audio_params_,
        OLIVE_RENDER_MODE_ONLINE
    );

    // 4. 连接完成信号
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, shm_name, shm_size](int exitCode, QProcess::ExitStatus status) {
        OnRenderProcessFinished(proc, shm_name, shm_size, exitCode, status);
    });
}

void ViewerWidget::OnRenderProcessFinished(QProcess* proc,
                                            const QString& shm_name,
                                            size_t shm_size,
                                            int exitCode,
                                            QProcess::ExitStatus status) {
    if (status == QProcess::CrashExit) {
        qWarning() << "Renderer process crashed for frame";
        // 不需要恢复状态，直接丢弃这一帧，UI 保持上一帧
        proc->deleteLater();
        return;
    }

    // 解析 stdout JSON
    QByteArray stdout_data = proc->readAllStandardOutput();
    QJsonObject result = QJsonDocument::fromJson(stdout_data).object();

    if (result["status"].toString() == "ok") {
        // 从共享内存读取帧
        void* shm_ptr = olive_shm_open(shm_name.toUtf8().constData(), shm_size);
        if (shm_ptr) {
            DisplayFrameFromShm(shm_ptr, result);
            olive_shm_close(shm_ptr, shm_size);
            olive_shm_unlink(shm_name.toUtf8().constData());
        }
    } else {
        qWarning() << "Render error:" << result["message"].toString();
    }

    proc->deleteLater();
}
```

---

## 8. 小步快跑实施步骤

### Step 0: 验证进程启动开销（1 天）

- [ ] 编写一个最小测试程序 `test_process_spawn.cpp`，只测量 `QProcess::start()` + `waitForStarted()` 的耗时。
- [ ] 加上 OpenGL 上下文初始化（`QOffscreenSurface` + `QOpenGLContext`）测量总耗时。
- [ ] 在目标平台（开发机）上测试，记录数据。

**验收标准**：得到精确的单进程启动耗时数据，作为是否采用"用完即弃"或"批处理模式"的依据。

### Step 1: 创建 olive-renderer 可执行文件目标（1 天）

- [ ] 新增 `app/render/renderer_main.cpp`。
- [ ] 在 `app/CMakeLists.txt` 中新增 `add_executable(olive-renderer ...)`。
- [ ] 链接 `oliverender`, `olivenode`, `olivecodec`, `olivecore`, `Qt::Core`。

**验收标准**：`olive-renderer` 编译成功，运行 `olive-renderer --help` 输出用法信息。

### Step 2: 实现命令行解析与节点图加载（1 天）

- [ ] 使用 `QCommandLineParser` 解析所有参数。
- [ ] 实现节点图 XML 加载（调用 `olive_node_graph_load_xml`）。

**验收标准**：`olive-renderer --node-graph=test.xml --output-shm=/test` 能成功加载节点图并找到 ViewerOutput。

### Step 3: 实现共享内存读写（1 天）

- [ ] 实现 `olive_shm_create/open/close/unlink`（POSIX + Windows）。
- [ ] 定义 `ShmHeader` 二进制布局。

**验收标准**：主进程创建 SHM，子进程写入数据，主进程读取并校验 CRC。

### Step 4: 单帧端到端渲染（2 天）

- [ ] 在 `olive-renderer` 中初始化 OpenGL 上下文。
- [ ] 调用 `olive_render_frame_sync` 渲染一帧。
- [ ] 将结果写入共享内存。
- [ ] 输出 JSON 到 stdout。
- [ ] 在主进程中编写 `RenderProcessLauncher::RenderFrameSync` 测试。

**验收标准**：主进程可以成功渲染一帧纯色/测试图，并在 Viewer 中显示。

### Step 5: 集成到 ViewerWidget（2 天）

- [ ] 修改 `ViewerWidget` 的帧请求逻辑，从直接调用 `RenderManager` 改为启动 `olive-renderer`。
- [ ] 处理异步完成信号，从共享内存读取帧并显示。
- [ ] 处理子进程崩溃（忽略该帧，保持上一帧显示）。

**验收标准**：拖动时间线时，Viewer 能实时显示渲染结果（可能有延迟，但功能正确）。

### Step 6: 批处理模式（2 天，按需）

- [ ] 若 Step 0 的测试显示单帧开销过高，实现 `--mode=batch`。
- [ ] 修改 `RenderProcessLauncher` 支持批处理。

### Step 7: 导出集成（2 天）

- [ ] 修改导出任务（`task/export/`），使用 `olive-renderer` 子进程逐帧渲染，然后编码。
- [ ] 导出天然适合批处理模式（可以一次性渲染 10–50 帧）。

---

## 9. 风险与回退

| 风险 | 对策 |
|---|---|
| 进程启动开销导致实时预览 < 10fps | 启用批处理模式，每 3–5 帧一个进程；或预启动一个进程池（但每个进程仍只服务一个批次后自杀）。 |
| OpenGL 驱动不支持离屏渲染 | 在 Linux 上使用 `EGL` 替代 `QOffscreenSurface`；在 Windows 上使用 `WGL` pbuffer；在 macOS 上使用 `CGL` pixel buffer。 |
| 共享内存名称冲突 | 使用 `QUuid::createUuid()` 生成唯一名称，格式为 `/olive_<pid>_<uuid>`。 |
| 共享内存泄漏（子进程崩溃后未 unlink） | 主进程在启动子进程前注册一个定时器，若子进程异常退出，5 秒后自动 `shm_unlink`。 |
| 磁盘空间不足（临时文件方案） | 渲染前检查磁盘空间，不足时返回错误码 6。 |
| 节点图 XML 过大导致解析慢 | 启用增量序列化：只序列化自上次以来变更的节点和参数。 |
| OFX 插件需要持久化状态 | OFX 插件实例不跨帧持久化，每帧重新创建。若某些插件初始化极慢，在 C API 中提供 `olive_plugin_instance_serialize_state` 接口，将状态快照传给下一帧的进程。 |
