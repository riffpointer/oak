# oakengine.so 迁移进度记录

## 当前状态

### 已完成（P4 暂停期间）

#### oakcodec.so C API 扩展
- `oak_decoder_create_from_id(id)`：按 ID（"ffmpeg"/"oiio"）创建 decoder wrapper
- `oak_decoder_id()` / `supports_video()` / `supports_audio()` / `is_open()`：能力查询
- `oak_decoder_probe_file(decoder, filepath, out_info)`：独立 probe 文件
- `oak_decoder_open_stream(decoder, filepath, stream_index)`：显式打开指定 stream
- `oak_decoder_read_video_ex()`：扩展视频解码（支持 divider、cancelled、force_range、renderer_hint）
- `oak_decoder_read_audio_ex()`：扩展音频解码（支持 loop_mode、render_mode、cache_path）
- `oak_decoder_conform_audio()`：触发音频 conform
- 代码风格改进：`std::malloc`/`std::free` → `new`/`delete[]`，使用 `auto*` 保持 C++ 风格

#### oakaudio.so C API 扩展
- `OakAudioParams` 扩展：新增 `sample_fmt`（OakAudioFormat）和 `channel_layout_mask`（uint64_t）
- `OakAudioFilterGraphHandle`：FFmpeg avfilter 音频处理图封装
  - `oak_audio_filter_graph_create(from, to, tempo)`
  - `oak_audio_filter_graph_destroy(graph)`
  - `oak_audio_filter_graph_process(graph, in_data, in_samples, out_data, out_samples, out_channels)`
  - `oak_audio_filter_graph_flush(graph, out_data, out_samples, out_channels)`
  - `oak_audio_filter_graph_free_output(data)`
- `oakaudio/CMakeLists.txt`：新增 `avfilter` + `avutil` 链接
- 代码风格改进：统一使用 `auto*` + RAII

### 阻塞问题

#### 1. OpenGLRendererProxy（高优先级）
- 需要创建 `OpenGLRendererProxy` 类，通过 `dlopen("liboakgl.dylib")` + `dlsym` 调用 `renderer_api.h` C API
- `Renderer` 虚函数使用 `QVariant` 传递 native handle，`OpenGLRendererProxy` 需把 `void*` 包装进 `QVariant`
- `PluginRenderer` 继承 `OpenGLRenderer`，直接依赖 Qt OpenGL。用户指示：PluginRenderer 类本身先不动，最终移入 `oakrenderer` 可执行文件

#### 2. Decoder/Encoder C++ 接口适配（中优先级）
- `oakcodec.so` C API 已扩展，但 engine 中 `clip.h`、`audiomanager.h`、`rendercache.h`、`renderprocessor.cpp` 等仍直接 include `decoder.h` 并使用 `DecoderPtr` C++ 类
- 下一步：在 engine 中创建 `DecoderProxy` / `EncoderProxy` C++ 适配器类，内部 `dlopen` `oakcodec.so` 并调用 C API
- 或者，把 `decoder.h` 等 C++ 头文件移入 `shared/include/`，让 engine 编译期可见（但 engine 与 oakcodec.so 之间仍需零编译期链接）

#### 3. FFmpeg 直接调用（engine 中）
- `pluginSupport/OliveClip.cpp`：直接 `#include <libavutil/pixdesc.h>`、`QOpenGLFunctions`（OFX plugin 相关）
- `audio/audioprocessor.cpp/h`：直接 `#include <libavfilter/avfilter.h>` 等 → **可由 `oakaudio.so` C API 替代**
- `render/plugin/pluginrenderer.cpp`：直接 `#include <libavutil/pixfmt.h>` 等 → 随 PluginRenderer 移出 engine

#### 4. OCIO 直接调用（engine 中）
- 用户指示：**color 可以保留在 engine 中**（OCIO ABI 稳定），无需强制拆出
- `render/colorprocessor.cpp`、`render/renderer.cpp`（GetColorContext）、`node/color/colormanager/` 等可保留编译期 OCIO 依赖

#### 5. OpenEXR 直接调用
- `render/framehashcache.cpp`：直接 `#include <OpenEXR/...>`
- 可以保留编译期依赖

## 下一步计划

### 回到 oakengine.so
1. 创建 `OpenGLRendererProxy`（dlopen oakgl.so）
2. 替换 engine 中旧 `OpenGLRenderer`
3. 恢复 `rendermanager.cpp` 实例化 `OpenGLRendererProxy`
4. 把 `audioprocessor.cpp` 改为调用 `oakaudio.so` C API
5. 逐步把 engine 中 `DecoderPtr` 使用改为 `oakcodec.so` C API（或创建 C++ Proxy 适配器）
6. 让 `src/engine/CMakeLists.txt` 恢复 OCIO/OIIO/OpenEXR 的编译期 include/link
