# oakengine.so 迁移与模块化状态记录（2026-05-25）

> 本文件记录从 monolithic engine 到多模块架构的实时状态。  
> 上次更新：2026-05-25

---

## 一、模块构建状态矩阵

| 模块 | 类型 | 构建状态 | C API 头文件 | C API 实现 | Runtime Loader |
|------|------|---------|-------------|-----------|----------------|
| `oakshared` | STATIC | ✅ 通过 | — | — | — |
| `oakgl.so` | SHARED | ✅ 通过 | `renderer_api.h` | ✅ `src/oakgl/` | ✅ `OakRendererRuntime` |
| `oakcodec.so` | SHARED | ✅ 通过 | `codec_api.h` + `frame_api.h` | ✅ `src/oakcodec/codec_api.cpp` | ✅ `OakCodecRuntime` |
| `oakaudio.so` | SHARED | ✅ 通过 | `audio_api.h` | ✅ `src/oakaudio/audio_api.cpp` | ❌ 无（需新建 `OakAudioRuntime`） |
| `oakcolor.so` | SHARED | ✅ 通过 | `color_api.h` | ✅ `src/oakcolor/` | 不需要 |
| `oakengine.so` | SHARED | ✅ 通过 | `core_api.h` `coord_api.h` `nodes_api.h` | ❌ **未实现导出** | — |
| `oakcore.so` | SHARED | ❌ **未接入 CMake** | `core_api.h` | ⚠️ `src/oakcore/core_api.cpp`（stub 实现，独立结构） | — |
| `oaknodes.so` | SHARED | ❌ **未接入 CMake** | `nodes_api.h` | ⚠️ `src/oaknodes/nodes_api.cpp`（纯 stub） | — |
| `oakcoord.so` | SHARED | ❌ **未接入 CMake** | `coord_api.h` | ⚠️ `src/oakcoord/coord_api.cpp`（stub 实现） | — |
| `oakrenderer` | EXECUTABLE | ❌ **未创建** | — | — | — |
| `oak-editor` | EXECUTABLE | ❌ 构建失败（OfxHost 的 libc++ 头文件路径问题） | — | — | — |

### 关键发现

- `src/oakcore/`、`src/oaknodes/`、`src/oakcoord/` **目录已存在**，内含 C API 实现代码，但**未被 `CMakeLists.txt` 编译**。
- 这些实现是**独立的简化结构**（`OakNodeGraph`、`OakNode`、`OakValue` 等），与 `src/engine/src/node/` 中现有的 Olive 节点系统（`olive::Node`、`olive::Project`、`olive::NodeTraverser`）**没有关联**。
- `oakengine.so` **当前未导出任何 C API 符号**（`nm -gU` 验证）。

---

## 二、FFmpeg 隔离进度

### 2.1 已完成 ✅

| 范围 | 状态 | 说明 |
|------|------|------|
| `shared/` | ✅ 完全隔离 | 已移除 `ffmpegutils.h/cpp`，`AudioParams` 改为 `uint64_t` mask，`SampleBuffer::rip_channel()` 移除 FFmpeg 调用 |
| `src/oakcodec/` | ✅ 唯一合法入口 | 所有 FFmpeg 调用封装在内部，`oak_frame_alloc/free/get_plane/get_params/convert` 等 C API 已导出 |
| `src/oakaudio/` | ✅ 独立 | `avfilter` 封装在内部 |
| `src/engine/src/`（除特定文件外） | ✅ 无直接 include | 全局 grep 确认：除 `avframe_types.h`、`pluginrenderer.cpp`、`OliveClip.cpp` 外，无其他 `#include <libav*>` / `#include <libsw*>` |

### 2.2 残余问题 🔴

| 文件 | 直接 FFmpeg 头文件包含 | 直接 FFmpeg API 调用 | 影响 |
|------|----------------------|---------------------|------|
| `src/engine/src/codec/avframe_types.h` | `#include <libavutil/frame.h>` | `av_frame_alloc()`, `av_frame_free()`, `av_frame_ref()` | Engine 内部仍暴露 `AVFrame` 类型 |
| `src/engine/src/render/plugin/pluginrenderer.cpp` | `libavutil/pixfmt.h`, `libavutil/pixdesc.h`, `libswscale/swscale.h` | `av_pix_fmt_desc_get`, `av_frame_get_buffer`, `sws_getContext`, `sws_scale`, `sws_freeContext`, `AV_PIX_FMT_*` | 直接调用 FFmpeg，约 41 处 |
| `src/engine/src/pluginSupport/OliveClip.cpp` | `libavutil/pixdesc.h`, `libswscale/swscale.h` | `av_pix_fmt_desc_get`, `av_frame_get_buffer`, `sws_getContext`, `sws_scale`, `sws_freeContext` | 直接调用 FFmpeg，约 17 处 |

**总计**：engine 内部仍有 **58 处**直接 FFmpeg API 调用（grep 统计）。

### 2.3 迁移路径

- `avframe_types.h`：最终需改为基于 `void*` 的 `FrameHandle`，通过 `oak_frame_*` C API 操作。
- `pluginrenderer.cpp` / `OliveClip.cpp`：
  - 将 `av_frame_get_buffer` 替换为 `oak_frame_alloc`
  - 将 `sws_getContext` + `sws_scale` + `sws_freeContext` 替换为 `oak_frame_convert`
  - 将 `av_pix_fmt_desc_get` + `AV_PIX_FMT_FLAG_PLANAR` 替换为 `video_format_is_planar`
  - 但 `AVFramePtr` 是 `std::shared_ptr<AVFrame>`，若改为 `void*`，需同步修改 `texture.h` 等所有使用点（约 215 处 `frame->` 访问）。

---

## 三、OpenGL 隔离进度

| 文件 | 状态 | 说明 |
|------|------|------|
| `src/engine/src/render/opengl/opengl_renderer_proxy.cpp/h` | ✅ 已完成 | 通过 `OakRendererRuntime` dlopen `oakgl.so`，所有虚函数通过 C API 转发 |
| `src/engine/src/render/rendermanager.cpp` | ✅ 已恢复 | `context_ = new OpenGLRendererProxy()` |
| `src/engine/src/render/renderprocessor.cpp` | ✅ 已清理 | `dynamic_cast<OpenGLRenderer*>` 已注释掉 |
| `src/engine/src/render/plugin/pluginrenderer.cpp` | 🟡 遗留 | `PluginRenderer` 的 `ConvertFrameIfNeeded` 中仍有注释掉的 `dynamic_cast<OpenGLRenderer*>`，但代码路径未实际执行 |

---

## 四、Codec Proxy 完成度

| Proxy 类 | 状态 | 说明 |
|----------|------|------|
| `DecoderProxy` (`decoder_proxy.h/cpp`) | ✅ 已完成 | 接口模拟原 `Decoder`，内部通过 `OakCodecRuntime` 调用 C API |
| `EncoderProxy` (`encoder_proxy.h/cpp`) | ✅ 已完成 | 接口模拟原 `FFmpegEncoder`，内部通过 `OakCodecRuntime` 调用 C API |
| `OakCodecRuntime` (`runtime/oak_codec_runtime.h/cpp`) | ✅ 已完成 | 加载 `liboakcodec.dylib/so`，绑定所有符号 |

**残余 dangling include**：
- `src/engine/src/node/output/viewer/viewer.h:25`：`#include "codec/encoder.h"`（已 broken，无实际使用，可安全删除）
- `src/engine/src/node/project/footage/footage.h:82`：注释中仍有 `Decoder::Probe()` 字样（实际代码已改为 `DecoderProxy`）

---

## 五、AudioProcessor 状态

| 文件 | 状态 | 说明 |
|------|------|------|
| `src/engine/src/audio/audioprocessor.h` | ✅ 已清理 | 无直接 `#include <libavfilter/avfilter.h>` |
| `src/engine/src/audio/audioprocessor.cpp` | ✅ 已清理 | 无直接 FFmpeg 调用 |
| `src/engine/src/audio/audiomanager.h/cpp` | ✅ 已清理 | 无直接 FFmpeg 调用 |
| `AudioProcessorProxy` | ❌ **未创建** | `oakaudio.so` C API 已完备，但 engine 中仍直接使用 `AudioProcessor` C++ 类（而非通过 Runtime Loader 调用） |

> 注：`AudioProcessor` 当前仍在 engine 内部编译，没有通过 `dlopen` 加载 `oakaudio.so`。若需完全切断编译期依赖，需创建 `AudioProcessorProxy` + `OakAudioRuntime`。

---

## 六、`oakrenderer` 可执行文件拆分 — 阻塞分析

### 6.1 目标

将渲染功能从 `oakengine.so` 拆出，成为**独立进程** `oakrenderer`：
- 不链接 `oakengine.so`
- 通过 C API 加载所需模块
- stdin/stdout JSON 协议 + 共享内存与主进程通信

### 6.2 当前阻塞点

#### 阻塞 1：`oakengine.so` 无 C API 导出 🔴🔴🔴

`nm -gU liboakengine.dylib` 验证：**零个 `oak_` 前缀的 C 符号**。

`oakrenderer` 需要调用 engine 的以下能力：
1. 节点图加载/反序列化（`Project::Load` 使用 XML，`renderer_process.md` 要求 JSON）
2. 节点图求值（`NodeTraverser::GenerateTable` / `RenderProcessor::Process`）
3. 渲染参数配置（`VideoParams`、`ColorManager`）

**方案对比**：

| 方案 | 描述 | 优劣 |
|------|------|------|
| A | 在 `oakengine.so` 中为现有 C++ 类添加 C 适配层，导出 C API | ✅ 最小化修改，复用现有节点系统<br>❌ 需在 engine 中维护 C 包装代码 |
| B | 继续完善 `src/oakcore/` 的独立实现，逐渐替换 engine 中的节点系统 | ✅ 长期架构更干净<br>❌ 工作量巨大，需重写 `NodeTraverser`、`RenderProcessor` 等核心逻辑 |
| C | `oakrenderer` 直接包含 `src/engine/src/node/` 和 `src/engine/src/render/` 源码 | ✅ 立即可行<br>❌ 代码重复编译，违背模块化原则 |

**用户倾向**：方案 A（C 适配层）。用户原话："在被调用的模块那里创建一个C适配层，用C函数包裹各个成员函数，然后用Handle来区分对象和替代this指针..."

#### 阻塞 2：节点图序列化格式不一致

- 现有 Olive 代码：`Project::Load/Save` 使用 **XML**（`QXmlStreamReader/Writer`）
- `renderer_process.md` 协议：要求 **JSON**（`load_graph` 命令接收 `graph_json`）
- `core_api.h` 定义了 `oak_graph_serialize/deserialize`，但 `oakengine.so` 未实现

**结论**：需要为 `Project`/`NodeGraph` 添加 JSON 序列化能力，或在 C API 层做 XML↔JSON 转换。

#### 阻塞 3：`oakcore.so` / `oaknodes.so` / `oakcoord.so` 未接入构建

`CMakeLists.txt` 中缺少：
```cmake
add_subdirectory(src/oakcore)
add_subdirectory(src/oaknodes)
add_subdirectory(src/oakcoord)
```

即使这些模块的 C API 实现完成，它们也不会被编译。

---

## 七、文件级改动清单（本次会话）

### 已修改（2026-05-25）

| 文件 | 改动 | 说明 |
|------|------|------|
| `shared/CMakeLists.txt` | 移除 FFmpeg | `ffmpegutils.cpp`、`FFMPEG_INCLUDE_DIRS`、`FFMPEG_LIBRARIES` 已移除 |
| `shared/include/olive/core/render/audioparams.h` | `AVChannelLayout` → `uint64_t` | 移除 `#include <libavutil/channel_layout.h>`，硬编码 layout masks |
| `src/engine/src/render/texture.h` | 未实际修改 | grep 显示仍为 `AVFramePtr`，与先前上下文记录不符，需复核 |
| `src/engine/src/codec/decoder_proxy.cpp` | 修复类型名 | `OAK_FRAME_PIX_FMT_RGBA8` → `OAK_FRAME_PIX_RGBA8` |
| `src/engine/src/codec/encoder_proxy.cpp` | 修复类型名 | 像素格式 enum 映射修正 |
| `src/engine/src/runtime/oak_codec_runtime.h/cpp` | 新增符号 | `video_format_to_av`, `av_to_video_format`, `video_format_is_planar`, `video_format_compatible`, `decoder_set_progress_callback`, `media_info_free` |
| `include/oak/codec_api.h` | 新增声明 | `oak_frame_alloc/free/get_plane/get_params/convert`，像素格式桥接函数 |
| `src/oakcodec/codec_api.cpp` | 新增实现 | `oak_frame_alloc/free/get_plane/get_params/convert` 等 C API 实现 |
| `src/engine/src/render/plugin/pluginrenderer.cpp` | `PackedDstInfo` 参数类型 | `AVPixelFormat` → `int` |
| `src/engine/src/render/plugin/pluginrenderer.cpp` | `ConvertFrameIfNeeded` 调用 | `this` → `nullptr`（因 `PluginRenderer` 不再继承 `Renderer`） |

### 待修改（已识别但未执行）

| 文件 | 待办 | 优先级 |
|------|------|--------|
| `src/engine/src/render/plugin/pluginrenderer.cpp` | 移除 FFmpeg 头文件包含，替换 `av_frame_get_buffer`→`oak_frame_alloc`，`sws_*`→`oak_frame_convert` | 高 |
| `src/engine/src/pluginSupport/OliveClip.cpp` | 同上 | 高 |
| `src/engine/src/codec/avframe_types.h` | 最终需改为 `void*` 基础框架 | 中 |
| `src/engine/src/render/texture.h` | `AVFramePtr` → `void*`（需同步修改 215 处 `frame->` 访问） | 中 |
| `src/engine/src/api/engine_api.cpp` | **新建**：为 `oakengine.so` 添加 C API 导出（`oak_render_service_*`） | 高（`oakrenderer` 阻塞） |
| `src/renderer/` | **新建目录**：`oakrenderer` 可执行文件 + JSON 协议 + 共享内存 | 高（`oakrenderer` 阻塞） |
| `CMakeLists.txt` | 接入 `src/oakcore`、`src/oaknodes`、`src/oakcoord`、`src/renderer` | 高 |

---

## 八、架构决策记录

### ADR 1：`oakrenderer` 与 `oakengine.so` 的关系

**决策**：`oakrenderer` 不链接 `oakengine.so`，运行时通过 `dlopen/dlsym` 调用其 C API。

**理由**：
- 符合"模块间全部显式加载调用 C 接口"原则
- `oakengine.so` 作为被调用方，需在其内部创建 C 适配层
- `oakrenderer` 作为调用方，需在其内部写适配器类封装 C API

### ADR 2：节点图 C API 的实现策略

**决策**：优先采用"C 适配层包裹现有 C++ 类"（方案 A），而非重写独立实现（方案 B）。

**理由**：
- 最小化修改
- 现有 `olive::Node`、`olive::Project`、`olive::NodeTraverser` 逻辑复杂，重写风险高
- `src/oakcore/` 的独立实现目前仅为 stub，尚未接入真实节点系统

### ADR 3：序列化格式

**待决策**：`oakrenderer` 的 `load_graph` 命令接收 JSON，但现有 Olive 使用 XML。

**选项**：
1. 在 `oakengine.so` 的 C API 层添加 `Project::toJson()` / `Project::fromJson()`
2. 主进程将 XML 序列化为 JSON 后传给 renderer
3. `oakrenderer` 直接接收 XML 字符串

---

## 九、下一步行动项（按优先级排序）

1. **🔴 为 `oakengine.so` 创建 C API 适配层**
   - 新建 `src/engine/src/api/engine_api.h` + `engine_api.cpp`
   - 导出：渲染服务创建/销毁、节点图加载、单帧渲染、帧范围渲染
   - 内部包装：`Project::Load`、`RenderProcessor::Process`、`Renderer` 操作

2. **🔴 创建 `oakrenderer` 可执行文件骨架**
   - 新建 `src/renderer/main.cpp` + `CMakeLists.txt`
   - 实现 `dlopen/dlsym` 加载 `oakengine.so`、`oakgl.so`、`oakcodec.so`
   - 实现 JSON 协议解析框架（`nlohmann/json` 或 Qt JSON）

3. **🟡 清理 engine 中残余 FFmpeg 直接调用**
   - `pluginrenderer.cpp`：替换为 `oak_frame_*` / `oak_frame_convert`
   - `OliveClip.cpp`：同上
   - 注意：`AVFramePtr` → `void*` 涉及 215 处修改，可分批进行

4. **🟡 接入 `oakcore` / `oaknodes` / `oakcoord` 到 CMake**
   - 修改顶层 `CMakeLists.txt`
   - 为各模块添加 `CMakeLists.txt`
   - 决策：这些模块是独立 `.so`，还是作为 `oakengine.so` 的一部分编译？

5. **🟢 修复 `oak-editor` 构建**
   - `OfxHost` 的 libc++ 头文件路径问题（可能是 Xcode SDK 升级导致）

6. **🟢 删除 dangling includes**
   - `viewer.h:25` 的 `#include "codec/encoder.h"`
   - `clip.h`、`traverser.h`、`viewer.h` 中未使用的 `#include "decoder.h"`
