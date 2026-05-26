# oakrenderer 进程通信协议

> `oakrenderer` 是一个**独立可执行文件**，链接 Qt，不链接 `oakengine.so`。
> 它只通过dlopen/dlsys加载：
> - `oakengine.so`（通过 C API 调用主编辑器能力）
> - `oakcodec.so`（通过 C API 解码素材）
> - `oakcolor.so`（通过 C API 色彩管理）
> - `oakgl.so`（通过 C API 渲染后端）
> - `oakaudio.so`（通过 C API 音频处理）
> - `oakpluginhost.so`（通过 C API OFX 插件）
>
> 与主进程的通信方式：
> - **stdin**：接收 JSON 格式的命令（主进程 → 渲染器）。
> - **stdout**：返回 JSON 格式的结果或事件（渲染器 → 主进程）。
> - **共享内存**：传输渲染好的帧数据（大体积二进制数据不走 stdio）。

## 一、进程启动

```bash
oakrenderer \
  --shm-prefix /oak_render_001 \
  --backend opengl \
  --max-memory-mb 2048
```

参数说明：
- `--shm-prefix`：共享内存段名称前缀，必须与主进程 `oak_coord_create` 时传入的前缀一致。
- `--backend`：渲染后端名称（"opengl"、"vulkan"、"cpu"）。
- `--max-memory-mb`：该进程允许使用的最大内存（MB），超出时主动退出。

渲染器启动后进入**空闲状态**，等待 stdin 上的命令。收到 `"cmd": "exit"` 或 EOF 时优雅退出。

## 二、命令协议（主进程 → 渲染器，JSON Lines）

> **全链路帧格式**：渲染器内部及共享内存传输的帧均为 **RGBA32F + ACEScg**（`OAK_FRAME_PIX_RGBA32F`，`colorspace = "ACES - ACEScg"`）。
> 预览窗口显示前必须通过 `oakcolor.so` 做 View Transform（RRT + ODT）转换到显示空间。

每行一个 JSON 对象，以 `\n` 结尾。主进程发送命令后，渲染器必须回复一行 JSON（异步事件除外）。

### 2.1 加载节点图

```json
{
  "cmd": "load_graph",
  "seq": 1,
  "graph_json": "{...节点图序列化字符串...}"
}
```

渲染器回复：
```json
{
  "seq": 1,
  "status": "ok",
  "node_count": 42
}
```

或失败：
```json
{
  "seq": 1,
  "status": "error",
  "error": "Unknown node type 'org.oak.SomeEffect'"
}
```

### 2.2 配置渲染参数

```json
{
  "cmd": "config",
  "seq": 2,
  "width": 1920,
  "height": 1080,
  "pixel_format": "rgba32f",
  "timebase_num": 24,
  "timebase_den": 1,
  "color_config_path": "/path/to/config.ocio",
  "input_color_space": "ACES - ACEScg",
  "display_color_space": "sRGB",
  "view_transform": "ACES 1.0 SDR-video"
}
```

渲染器回复：
```json
{
  "seq": 2,
  "status": "ok"
}
```

### 2.3 渲染帧范围

```json
{
  "cmd": "render_range",
  "seq": 3,
  "start_frame": 100,
  "end_frame": 199,
  "frame_step": 1
}
```

渲染器收到后**立即回复确认**，然后开始异步渲染：
```json
{
  "seq": 3,
  "status": "accepted",
  "job_id": 3
}
```

渲染过程中，渲染器通过 stdout 主动推送进度事件（见第 3 节）。

### 2.4 渲染单帧（同步）

```json
{
  "cmd": "render_frame",
  "seq": 4,
  "frame_number": 150
}
```

渲染器回复（包含帧元数据，但**不包含像素数据**，像素数据走共享内存）：
```json
{
  "seq": 4,
  "status": "ok",
  "frame_number": 150,
  "shm_segment": "/oak_render_001_frame_150",
  "shm_size": 33177600,
  "width": 1920,
  "height": 1080,
  "pix_fmt": "rgba32f",
  "colorspace": "ACES - ACEScg",
  "stride": 7680
}
```

@note `pix_fmt` 固定为 `"rgba32f"`，`colorspace` 固定为 `"ACES - ACEScg"`。

### 2.5 查询帧状态

```json
{
  "cmd": "query_frame",
  "seq": 5,
  "frame_number": 150
}
```

### 2.6 取消当前任务

```json
{
  "cmd": "cancel",
  "seq": 6
}
```

### 2.7 释放帧内存

```json
{
  "cmd": "release_frame",
  "seq": 7,
  "frame_number": 150
}
```

渲染器收到后，释放该帧对应的共享内存段。

### 2.8 优雅退出

```json
{
  "cmd": "exit",
  "seq": 8
}
```

渲染器完成当前正在渲染的帧后退出，回复：
```json
{
  "seq": 8,
  "status": "ok"
}
```

## 三、事件协议（渲染器 → 主进程，JSON Lines）

渲染器主动推送到 stdout，不需要等待主进程请求。

### 3.1 进度事件

```json
{
  "event": "progress",
  "job_id": 3,
  "frame_number": 105,
  "completed": 6,
  "total": 100
}
```

### 3.2 帧完成事件

```json
{
  "event": "frame_done",
  "job_id": 3,
  "frame_number": 105,
  "shm_segment": "/oak_render_001_frame_105",
  "shm_size": 33177600,
  "width": 1920,
  "height": 1080,
  "pix_fmt": "rgba32f",
  "colorspace": "ACES - ACEScg",
  "stride": 7680,
  "render_time_ms": 45
}
```

### 3.3 帧失败事件

```json
{
  "event": "frame_failed",
  "job_id": 3,
  "frame_number": 110,
  "error": "Shader compilation failed: ..."
}
```

### 3.4 任务完成事件

```json
{
  "event": "job_done",
  "job_id": 3,
  "completed": 98,
  "failed": 2,
  "total": 100,
  "total_time_ms": 4520
}
```

### 3.5 内存警告事件

```json
{
  "event": "memory_warning",
  "used_mb": 1800,
  "limit_mb": 2048
}
```

## 四、共享内存协议

### 4.1 命名规则

共享内存段名称格式：
```
{shm_prefix}frame_{frame_number}
```

例如：`/oak_render_001_frame_150`

### 4.2 生命周期

1. 渲染器渲染完一帧后，创建共享内存段并写入像素数据。
2. 渲染器通过 stdout 发送 `frame_done` 事件，告知主进程段名称和大小。
3. 主进程通过 `mmap`（POSIX）或 `MapViewOfFile`（Windows）映射该段，读取像素数据。
4. 主进程读取完毕后，发送 `release_frame` 命令。
5. 渲染器收到 `release_frame` 后，`munmap` 并 `shm_unlink`（POSIX）或关闭句柄。

### 4.3 内存布局

共享内存段前 256 字节为**元数据头**（避免 JSON 中传递大体积二进制）：

```c
typedef struct {
    uint32_t magic;           // 'OAK\0' = 0x4F414B00
    uint32_t version;         // 1
    uint32_t width;
    uint32_t height;
    uint32_t pix_fmt;         // 固定为 OAK_FRAME_PIX_RGBA32F
    uint32_t colorspace_tag;  // 0 = ACEScg (默认), 1 = sRGB, 2 = Rec.2020...
    uint32_t stride;
    uint64_t timestamp_ns;    // 渲染完成时间戳
    uint64_t reserved[5];     // 保留
} OakFrameShmHeader;          // 64 字节对齐
```

元数据头之后紧跟像素数据：`stride * height` 字节。

### 4.4 平台差异

| 平台 | API | 说明 |
|------|-----|------|
| Linux / macOS | `shm_open` + `mmap` | POSIX 共享内存 |
| Windows | `CreateFileMapping` + `MapViewOfFile` | 命名文件映射 |

渲染器内部使用 `#ifdef _WIN32` 区分实现，但对外协议完全一致。

## 五、错误处理与超时

1. **命令超时**：主进程发送命令后，若 5 秒内未收到回复，视为渲染器卡死，主进程应 `kill` 该进程并启动新渲染器。
2. **渲染超时**：单帧渲染超过 30 秒，渲染器自动发送 `frame_failed` 事件并跳过该帧。
3. **崩溃恢复**：若渲染器进程崩溃（stdout 断开），主进程通过共享内存段前缀清理残留的 `shm_unlink`，然后启动新渲染器重试失败帧。

## 六、完整交互示例

```
[主进程 stdin → 渲染器]
{"cmd":"load_graph","seq":1,"graph_json":"{...}"}
{"cmd":"config","seq":2,"width":1920,"height":1080,...}
{"cmd":"render_range","seq":3,"start_frame":100,"end_frame":199}

[渲染器 stdout → 主进程]
{"seq":1,"status":"ok","node_count":42}
{"seq":2,"status":"ok"}
{"seq":3,"status":"accepted","job_id":3}
{"event":"progress","job_id":3,"frame_number":100,"completed":1,"total":100}
{"event":"frame_done","job_id":3,"frame_number":100,"shm_segment":"/oak_001_frame_100",...}
...
{"event":"job_done","job_id":3,"completed":100,"failed":0,"total":100,...}

[主进程 stdin → 渲染器]
{"cmd":"release_frame","seq":4,"frame_number":100}
{"cmd":"exit","seq":5}
```
