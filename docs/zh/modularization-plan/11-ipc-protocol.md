# IPC 协议规范

> 本文件定义主进程与 `olive-renderer` 子进程之间的全部通信方式。由于采用**"用完即弃"**模型，协议被设计为**极简、无状态、单向请求-响应**。

---

## 1. 通信通道概览

| 通道 | 方向 | 用途 | 格式 |
|---|---|---|---|
| **命令行参数** | 主进程 → 子进程 | 传递渲染配置（模式、路径、参数） | POSIX 风格长选项 |
| **stdin** | 主进程 → 子进程 | 传递节点图 XML（当 XML 过大超出命令行长度限制时） | 原始 XML 字符串 |
| **stdout** | 子进程 → 主进程 | 返回渲染结果元数据 | 单行 JSON |
| **stderr** | 子进程 → 主进程 | 日志和详细错误信息 | 纯文本 |
| **共享内存 / 内存映射文件** | 双向 | 传输大帧数据（像素/采样） | 二进制（ShmHeader + 原始数据） |
| **退出码** | 子进程 → 主进程 | 快速判断成功/失败/异常 | 整数 (0–255) |

**重要**：由于子进程渲染完成后立即退出，不存在**长期状态同步**、**心跳**、**取消信号**（主进程直接 `kill` 即可）。

---

## 2. 命令行参数

### 2.1 参数总表

| 参数 | 类型 | 必需 | 默认值 | 说明 |
|---|---|---|---|---|
| `--mode` | string | 否 | `frame` | 渲染模式：`frame`, `batch`, `audio` |
| `--node-graph` | path | 是 | — | 节点图 XML 文件路径 |
| `--node-graph-stdin` | flag | 否 | false | 从 stdin 读取节点图 XML，而非文件 |
| `--output-node` | string | 否 | 自动探测 | 输出节点 ID |
| `--time` | rational | 条件 | — | 单帧时间点（`--mode=frame` 时必需） |
| `--times` | csv | 条件 | — | 多帧时间点列表（`--mode=batch` 时必需） |
| `--start` | rational | 条件 | — | 音频起始时间（`--mode=audio` 时必需） |
| `--duration` | rational | 条件 | — | 音频持续时间（`--mode=audio` 时必需） |
| `--video-params` | json | 条件 | — | 视频参数（`--mode=frame`/`batch` 时必需） |
| `--audio-params` | json | 条件 | — | 音频参数（`--mode=audio` 时必需） |
| `--color-ref` | string | 否 | — | 参考色彩空间名称 |
| `--color-display` | string | 否 | — | 显示色彩空间名称 |
| `--force-size` | json | 否 | — | 强制输出尺寸，如 `{"width":1920,"height":1080}` |
| `--force-format` | string | 否 | — | 强制像素格式，如 `rgba32f` |
| `--output-shm` | string | 是 | — | 输出共享内存名称或临时文件路径 |
| `--output-shm-size` | int | 是 | — | 输出缓冲区大小（字节） |
| `--output-stdout` | flag | 否 | false | 将帧数据 base64 编码输出到 stdout（仅小帧/测试） |
| `--backend` | string | 否 | `opengl` | 渲染后端：`opengl`, `dummy` |
| `--shader-path` | path | 否 | `<exe_dir>/shaders` | 着色器资源目录 |
| `--ocio-config` | path | 否 | — | OCIO 配置文件路径 |
| `--verbose` | flag | 否 | false | 详细日志输出到 stderr |
| `--version` | flag | 否 | false | 输出版本信息并退出 |
| `--help` | flag | 否 | false | 输出帮助信息并退出 |

### 2.2 参数值格式

**Rational**：`"<numerator>/<denominator>"`，如 `"1001/30000"`, `"0/1"`。

**JSON**：紧凑格式，键用双引号。例如：
```
--video-params='{"width":1920,"height":1080,"format":"rgba32f","channel_count":4,"depth":1}'
```

**CSV**：逗号分隔的有理数字符串。例如：
```
--times="0/24,1/24,2/24,3/24,4/24"
```

---

## 3. 标准输入（stdin）

当 `--node-graph-stdin` 标志存在时，子进程从 stdin 读取节点图 XML，而不是从 `--node-graph` 指定的文件。

### 3.1 使用场景

- 节点图 XML 非常大（> 100KB），超出命令行长度限制。
- 主进程不想在磁盘上创建临时文件。
- 安全考虑：敏感项目数据不写入磁盘。

### 3.2 协议

```
主进程          子进程
  │               │
  │── XML 数据 ──>│（子进程读取 stdin 直到 EOF）
  │               │
```

子进程读取 stdin 的全部内容，视为节点图 XML 字符串。XML 结束后不需要特殊分隔符（EOF 即结束）。

**注意**：由于子进程使用 `QCoreApplication` 且不使用 Qt 的事件循环读取 stdin，应使用阻塞式 `QTextStream(stdin).readAll()` 或 `std::cin`。

---

## 4. 标准输出（stdout）

子进程将渲染结果以**单行 JSON** 输出到 stdout，以换行符 `\n` 结尾。主进程读取第一行后即视为响应完成。

### 4.1 单帧模式输出（`--mode=frame`）

**成功：**
```json
{
  "status": "ok",
  "mode": "frame",
  "time": "1001/30000",
  "width": 1920,
  "height": 1080,
  "format": "rgba32f",
  "pixel_format_id": 28,
  "channel_count": 4,
  "shm_name": "/olive_frame_abc123",
  "data_offset": 256,
  "data_size": 33177600,
  "linesize": 7680,
  "render_time_ms": 42
}
```

**失败：**
```json
{
  "status": "error",
  "mode": "frame",
  "time": "1001/30000",
  "error_code": "decoder_failure",
  "message": "Failed to open decoder for footage 'clip001.mp4'",
  "render_time_ms": 5
}
```

### 4.2 批处理模式输出（`--mode=batch`）

**成功：**
```json
{
  "status": "ok",
  "mode": "batch",
  "frame_count": 5,
  "frames": [
    {"time": "0/24", "data_offset": 256, "data_size": 33177600, "render_time_ms": 45},
    {"time": "1/24", "data_offset": 33178056, "data_size": 33177600, "render_time_ms": 38},
    {"time": "2/24", "data_offset": 66356112, "data_size": 33177600, "render_time_ms": 41},
    {"time": "3/24", "data_offset": 99534168, "data_size": 33177600, "render_time_ms": 39},
    {"time": "4/24", "data_offset": 132712224, "data_size": 33177600, "render_time_ms": 42}
  ],
  "total_render_time_ms": 205
}
```

### 4.3 音频模式输出（`--mode=audio`）

**成功：**
```json
{
  "status": "ok",
  "mode": "audio",
  "start": "0/1",
  "duration": "48000/48000",
  "sample_rate": 48000,
  "channels": 2,
  "sample_count": 48000,
  "shm_name": "/olive_audio_abc123",
  "data_offset": 256,
  "data_size": 384000,
  "render_time_ms": 15
}
```

### 4.4 字段说明

| 字段 | 类型 | 出现条件 | 说明 |
|---|---|---|---|
| `status` | string | 总是 | `"ok"`, `"error"`, `"cancelled"` |
| `mode` | string | 总是 | `"frame"`, `"batch"`, `"audio"` |
| `time` / `times` | string | frame/batch | 渲染时间点 |
| `width` | int | frame/batch | 帧宽 |
| `height` | int | frame/batch | 帧高 |
| `format` | string | frame/batch | 像素格式名称 |
| `pixel_format_id` | int | frame/batch | 像素格式枚举值 |
| `channel_count` | int | frame/batch | 通道数 |
| `shm_name` | string | 总是 | 共享内存名称 |
| `data_offset` | int | 总是 | 实际数据在共享内存中的偏移（跳过 ShmHeader） |
| `data_size` | int | 总是 | 实际数据大小（字节） |
| `linesize` | int | frame/batch | 每行字节数（含 padding） |
| `frame_count` | int | batch | 批处理帧数 |
| `frames` | array | batch | 每帧的元数据 |
| `sample_rate` | int | audio | 采样率 |
| `channels` | int | audio | 通道数 |
| `sample_count` | int | audio | 采样数 |
| `error_code` | string | error | 错误分类码 |
| `message` | string | error | 人类可读错误信息 |
| `render_time_ms` | int | 总是 | 纯渲染耗时（不含进程启动） |

---

## 5. 共享内存二进制布局

### 5.1 整体结构

```
┌─────────────────────────────────────────────────────────────────┐
│                        共享内存区域                              │
├────────────────────────┬────────────────────────────────────────┤
│    ShmHeader (256 B)   │            Payload Data                │
│                        │                                        │
│  magic                 │  帧像素数据 / 音频采样数据              │
│  version               │                                        │
│  data_offset           │  大小 = data_size                      │
│  data_size             │                                        │
│  width                 │                                        │
│  height                │                                        │
│  format                │                                        │
│  linesize              │                                        │
│  checksum              │                                        │
│  reserved[...]         │                                        │
└────────────────────────┴────────────────────────────────────────┘
```

### 5.2 ShmHeader 定义（C 结构）

```c
#include <stdint.h>

#define OLIVE_SHM_MAGIC     0x4F4C4956  // 'OLIV' 大端序
#define OLIVE_SHM_VERSION   1
#define OLIVE_SHM_HEADER_SIZE 256

typedef struct {
    uint32_t magic;           // OLIVE_SHM_MAGIC
    uint32_t version;         // OLIVE_SHM_VERSION
    uint32_t data_offset;     // Payload 数据起始偏移（>= 256）
    uint64_t data_size;       // Payload 实际数据大小（字节）
    uint32_t width;           // 帧宽 或 采样数
    uint32_t height;          // 帧高 或 0（音频）
    uint32_t depth;           // 3D 纹理深度（通常 1）
    uint32_t channel_count;   // 通道数
    uint32_t pixel_format;    // OlivePixelFormat 枚举值
    uint32_t linesize;        // 每行字节数（视频）或 0（音频）
    uint64_t checksum;        // CRC64 校验和（可选，0 表示未校验）
    uint8_t  reserved[256 - 48];  // 填充至 256 字节，未来扩展用
} OliveShmHeader;

// 辅助：计算 CRC64
uint64_t olive_shm_checksum(const void* data, size_t size);
```

### 5.3 校验和（Checksum）

- 默认启用 CRC64 校验。
- `checksum` 字段覆盖 **Payload Data 区域**（从 `data_offset` 开始的 `data_size` 字节）。
- 若主进程设置 `checksum = 0`，表示跳过校验（用于调试或性能敏感场景）。

### 5.4 多帧批处理布局

批处理模式下，所有帧连续存储在同一块共享内存中：

```
Offset 0:      ShmHeader (256 B)
Offset 256:    Frame 0 data (frame_0_size B, 按 linesize 对齐)
Offset 256+N0: Frame 1 data (frame_1_size B)
Offset 256+N0+N1: Frame 2 data
...
```

每帧的 `data_offset` 在 stdout JSON 中单独指定。

---

## 6. 标准错误（stderr）

### 6.1 日志级别

当 `--verbose` 启用时，stderr 输出结构化日志：

```
[2024-05-21T10:30:15.123Z] [INFO] 初始化 OpenGL 上下文
[2024-05-21T10:30:15.245Z] [INFO] OpenGL 版本: 4.6.0 NVIDIA 535.104
[2024-05-21T10:30:15.310Z] [INFO] 加载节点图: 42 个节点
[2024-05-21T10:30:15.412Z] [INFO] 开始渲染帧 @ 1001/30000
[2024-05-21T10:30:15.454Z] [INFO] 渲染完成, 耗时 42ms
[2024-05-21T10:30:15.455Z] [INFO] 写入共享内存: /olive_frame_abc123, 33177600 bytes
```

### 6.2 错误日志

错误同时输出到 stderr 和 stdout JSON：

```
[2024-05-21T10:30:15.456Z] [ERROR] Decoder 初始化失败: codec not found for 'hevc'
```

---

## 7. 退出码

| 退出码 | 名称 | 含义 | 主进程应对 |
|---|---|---|---|
| 0 | `EXIT_OK` | 渲染成功 | 正常处理结果 |
| 1 | `EXIT_GENERIC_ERROR` | 通用错误 | 记录错误，丢弃该帧 |
| 2 | `EXIT_INVALID_ARGS` | 命令行参数无效 | 检查主进程参数组装逻辑 |
| 3 | `EXIT_INIT_FAILED` | 初始化失败（OpenGL/OCIO） | 尝试 `dummy` 后端或提示用户 |
| 4 | `EXIT_GRAPH_LOAD_FAILED` | 节点图加载/解析失败 | 检查 XML 序列化逻辑 |
| 5 | `EXIT_RENDER_FAILED` | 渲染过程中出错 | 记录具体错误，丢弃该帧 |
| 6 | `EXIT_OUTPUT_FAILED` | 输出写入失败（SHM 不足） | 清理 SHM，重试或报错 |
| 130 | `EXIT_SIGINT` | 收到 SIGINT（Ctrl+C / kill -2） | 视为取消，正常丢弃 |
| 137 | `EXIT_SIGKILL` | 收到 SIGKILL（kill -9） | 视为取消，正常丢弃 |
| 139 | `EXIT_SEGFAULT` | 段错误（未捕获信号） | 视为崩溃，记录日志 |
| 其他 | — | 未知错误 | 记录日志，丢弃该帧 |

---

## 8. 主进程与子进程交互时序

### 8.1 单帧完整时序

```
时间轴 ──────────────────────────────────────────────────────────────>

主进程:     [准备参数]  [创建SHM]  [QProcess::start()]  [等待]  [读JSON]  [读SHM]  [unlink SHM]
                │           │            │                │        │        │        │
子进程:                  [启动]  [解析参数]  [加载XML]  [初始化GL]  [渲染]  [写SHM]  [写stdout]  [exit]
                             │        │        │           │        │        │          │
                             └────────┴────────┴───────────┴────────┴────────┴──────────┘
                                                   进程生命周期
```

### 8.2 异常情况时序

**子进程崩溃（segfault）**：
```
主进程: [start] ── [wait] ── [finished信号] ── [exitStatus == CrashExit] ── [忽略该帧]
子进程: [启动] ── [崩溃] ──────────────────── [操作系统回收资源]
```

**主进程取消（kill）**：
```
主进程: [start] ── [用户操作/超时] ── [QProcess::kill()] ── [finished信号] ── [unlink SHM]
子进程: [启动] ── [渲染中] ───────── [SIGKILL] ──────────── [立即终止]
```

---

## 9. 共享内存生命周期管理

### 9.1 创建

```cpp
// 主进程创建
QString shm_name = "/olive_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
size_t shm_size = CalculateFrameSize(params) + OLIVE_SHM_HEADER_SIZE;
void* shm_ptr = olive_shm_create(shm_name.toUtf8().constData(), shm_size);
```

### 9.2 命名规范

- POSIX: `/olive_<uuid>`，必须以 `/` 开头，长度 < 255。
- Windows: `Local\\olive_<uuid>` 或 `Global\\olive_<uuid>`。
- 临时文件: `/tmp/olive_<pid>_<uuid>.raw`（内存映射临时文件回退方案）。

### 9.3 清理策略

**正常路径**：
1. 子进程成功渲染，写入数据，退出。
2. 主进程读取数据。
3. 主进程 `olive_shm_close(ptr, size)` + `olive_shm_unlink(name)`。

**异常路径（子进程崩溃）**：
1. 主进程检测到 `QProcess::CrashExit`。
2. 主进程立即 `olive_shm_unlink(name)`（即使数据未读取）。

**双重保险**：
- 主进程在创建 SHM 时启动一个 `QTimer`（5 秒后触发）。
- 若 5 秒后 SHM 仍未被清理（异常路径未执行到 unlink），定时器自动 `olive_shm_unlink(name)`。
- 防止子进程崩溃后主进程也崩溃导致的 SHM 泄漏。

---

## 10. 平台差异

### 10.1 Linux

- 共享内存：`/dev/shm/` 下的 tmpfs 文件。
- 最大名称长度：255 字节（含 null）。
- 权限：`shm_open` 使用 `0666`，确保子进程可以打开。
- 系统限制：`/proc/sys/kernel/shmmax` 通常足够大（> 1GB）。

### 10.2 macOS

- 共享内存：`shm_open` 创建的 POSIX 共享内存对象。
- 注意：macOS 的 `shm_open` 名称长度限制为 31 字符（包括开头的 `/`）！
- **解决方案**：使用 **内存映射临时文件** 替代 POSIX shm。

```cpp
// macOS 专用：使用临时文件替代 shm_open
int fd = mkstemp("/tmp/olive_XXXXXX.raw");
ftruncate(fd, size);
void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
// 将文件路径传递给子进程
```

### 10.3 Windows

- 共享内存：`CreateFileMapping` + `MapViewOfFile`。
- 名称：`Local\\olive_<uuid>`（用户会话内可见）。
- 注意：句柄管理。子进程 `MapViewOfFile` 后需要保留 `HANDLE` 以便后续 `UnmapViewOfFile` 和 `CloseHandle`。
- 简化方案：子进程只负责写入，不关闭映射。进程退出后操作系统自动回收。

---

## 11. 性能调优建议

### 11.1 减少进程启动开销

| 技术 | 效果 | 复杂度 |
|---|---|---|
| 静态链接 `olive-renderer` | 避免动态库加载开销 | 低 |
| 预加载 Qt 插件 | 减少 `QCoreApplication` 初始化时间 | 低 |
| 批处理模式 | 摊销 OpenGL 上下文创建开销 | 中 |
| 使用 `EGL` 替代 `GLX`/`WGL` | EGL 上下文创建更快 | 中 |

### 11.2 减少序列化开销

| 技术 | 效果 | 复杂度 |
|---|---|---|
| 节点图 XML 缓存 | 相同图只序列化一次 | 低 |
| 增量参数更新 | 仅发送变更的参数 | 中 |
| 二进制序列化格式 | 替代 XML，解析更快 | 高 |

### 11.3 减少共享内存开销

| 技术 | 效果 | 复杂度 |
|---|---|---|
| 共享内存池 | 预分配 N 块循环使用 | 中 |
| 内存映射临时文件 | 避免 `shm_open` 系统调用 | 低 |
| 零拷贝（GPU 纹理共享） | 跨进程直接共享 GPU 纹理 | 高（平台相关） |

---

## 12. 调试工具

### 12.1 手动运行子进程

```bash
# 直接运行 olive-renderer，独立于主进程
./olive-renderer \
  --mode=frame \
  --node-graph=/tmp/debug_graph.xml \
  --time=0/1 \
  --video-params='{"width":100,"height":100,"format":"rgba32f"}' \
  --output-shm=/olive_debug \
  --output-shm-size=160000 \
  --verbose

# 查看 stdout 输出
# 查看 stderr 日志
# 用另一个程序读取 /olive_debug 验证像素数据
```

### 12.2 环境变量

| 变量 | 作用 |
|---|---|
| `OLIVE_RENDERER_BACKEND=dummy` | 强制使用 dummy 后端（跳过 OpenGL） |
| `OLIVE_RENDERER_TIMEOUT=60000` | 子进程超时时间（毫秒） |
| `OLIVE_RENDERER_KEEP_SHM=1` | 子进程退出后不删除共享内存（调试） |
| `OLIVE_RENDERER_LOG_FILE=/path/to.log` | 将日志写入文件 |

### 12.3 重放渲染

将主进程发送给子进程的所有输入（命令行参数 + XML）保存到日志目录，可以精确重放某一帧的渲染：

```bash
# 主进程日志目录：~/.local/share/oak/renderer_logs/
# 每个渲染任务保存为：
#   frame_12345.params  (命令行参数)
#   frame_12345.xml     (节点图)

# 重放
./olive-renderer $(cat ~/.local/share/oak/renderer_logs/frame_12345.params)
```
