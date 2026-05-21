# libolivecodec.so — 编解码库

> **依赖**：`libolivecore.so`  
> **外部依赖**：FFmpeg (avcodec, avformat, avutil, swscale, swresample, avfilter), OpenImageIO, OpenEXR  
> **包含源码**：`app/codec/`, `app/common/`  
> **当前状态**：单体 OBJECT 库的一部分  
> **改造难度**：⭐⭐（较简单）

---

## 1. 当前状态分析

`app/codec/` 负责媒体文件的读取与写入，`app/common/` 提供通用工具（FFmpeg 辅助、XML 工具、文件操作等）。两者紧密耦合，且 `common/` 被 `codec/` 重度依赖，因此合并为一个动态库。

| 组件 | 说明 |
|---|---|
| `decoder.h/cpp` | 解码器抽象基类 |
| `ffmpeg/ffmpegdecoder` / `ffmpegencoder` | FFmpeg 视频/音频解码编码 |
| `oiio/oiiodecoder` / `oiioencoder` | OpenImageIO 图像序列解码编码 |
| `frame.h/cpp` | CPU 帧数据（`FramePtr`） |
| `stream.h` | 媒体流信息 |
| `conformmanager.h/cpp` | 音频格式统一转换 |
| `common/ffmpegutils.h` | FFmpeg 辅助函数 |
| `common/xmlutils.h` | XML 序列化辅助 |
| `common/filefunctions.h` | 文件操作 |

---

## 2. C API 设计

### 2.1 头文件：`c_api/include/olive/codec_api.h`

```c
#ifndef OLIVE_CODEC_API_H
#define OLIVE_CODEC_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core_api.h"

#define OLIVE_CODEC_API_VERSION 1

#ifdef OLIVE_BUILDING_CODEC
#  define OLIVE_CODEC_API __attribute__((visibility("default")))
#else
#  define OLIVE_CODEC_API
#endif

/* ========== 类型前向声明 ========== */
typedef struct OliveDecoder   OliveDecoder;
typedef struct OliveEncoder   OliveEncoder;
typedef struct OliveFrame     OliveFrame;
typedef struct OliveStream    OliveStream;
typedef struct OliveMediaInfo OliveMediaInfo;

/* ========== API 版本 ========== */
OLIVE_CODEC_API int olive_codec_api_version(void);

/* ========== MediaInfo（媒体文件信息） ========== */
OLIVE_CODEC_API OliveMediaInfo* olive_media_info_probe(const char* filename);
OLIVE_CODEC_API void olive_media_info_destroy(OliveMediaInfo* info);

OLIVE_CODEC_API int olive_media_info_stream_count(OliveMediaInfo* info);
OLIVE_CODEC_API int olive_media_info_stream_type(OliveMediaInfo* info, int stream_index);  // 0=video, 1=audio, 2=subtitle
OLIVE_CODEC_API OliveVideoParams olive_media_info_video_params(OliveMediaInfo* info, int stream_index);
OLIVE_CODEC_API OliveAudioParams olive_media_info_audio_params(OliveMediaInfo* info, int stream_index);
OLIVE_CODEC_API OliveRational olive_media_info_duration(OliveMediaInfo* info);
OLIVE_CODEC_API const char* olive_media_info_codec_name(OliveMediaInfo* info, int stream_index);

/* ========== Decoder ========== */
OLIVE_CODEC_API OliveDecoder* olive_decoder_create(const char* codec_id);
OLIVE_CODEC_API void olive_decoder_destroy(OliveDecoder* decoder);

OLIVE_CODEC_API int olive_decoder_open(OliveDecoder* decoder,
                                        const char* filename,
                                        int stream_index);
OLIVE_CODEC_API void olive_decoder_close(OliveDecoder* decoder);

// 视频解码：解码指定时间的帧
OLIVE_CODEC_API int olive_decoder_decode_video(OliveDecoder* decoder,
                                                OliveRational time,
                                                OliveFrame** out_frame);

// 音频解码：解码指定时间范围的采样
OLIVE_CODEC_API int olive_decoder_decode_audio(OliveDecoder* decoder,
                                                OliveRational start,
                                                OliveRational duration,
                                                OliveSampleBuffer** out_buffer);

// 获取解码器支持的流参数
OLIVE_CODEC_API OliveVideoParams olive_decoder_video_params(OliveDecoder* decoder);
OLIVE_CODEC_API OliveAudioParams olive_decoder_audio_params(OliveDecoder* decoder);

/* ========== Frame ========== */
OLIVE_CODEC_API void olive_frame_destroy(OliveFrame* frame);

OLIVE_CODEC_API int olive_frame_width(OliveFrame* frame);
OLIVE_CODEC_API int olive_frame_height(OliveFrame* frame);
OLIVE_CODEC_API int olive_frame_linesize(OliveFrame* frame);
OLIVE_CODEC_API OlivePixelFormat olive_frame_format(OliveFrame* frame);
OLIVE_CODEC_API void* olive_frame_data(OliveFrame* frame);  // 指向像素数据的指针
OLIVE_CODEC_API size_t olive_frame_data_size(OliveFrame* frame);

// 将 Frame 转换为指定的像素格式（内部使用 swscale）
OLIVE_CODEC_API int olive_frame_convert(OliveFrame* src,
                                         OlivePixelFormat dst_format,
                                         OliveFrame** out_frame);

// 从原始数据创建 Frame（用于渲染结果回传）
OLIVE_CODEC_API OliveFrame* olive_frame_from_data(int width,
                                                   int height,
                                                   OlivePixelFormat format,
                                                   const void* data,
                                                   int linesize);

/* ========== Encoder ========== */
OLIVE_CODEC_API OliveEncoder* olive_encoder_create(const char* format_name,
                                                    const char* codec_name);
OLIVE_CODEC_API void olive_encoder_destroy(OliveEncoder* encoder);

OLIVE_CODEC_API int olive_encoder_open(OliveEncoder* encoder,
                                        const char* filename,
                                        OliveVideoParams vparams,
                                        OliveAudioParams aparams);
OLIVE_CODEC_API int olive_encoder_write_video(OliveEncoder* encoder, OliveFrame* frame);
OLIVE_CODEC_API int olive_encoder_write_audio(OliveEncoder* encoder, OliveSampleBuffer* buffer);
OLIVE_CODEC_API int olive_encoder_close(OliveEncoder* encoder);

/* ========== Conform（音频格式统一） ========== */
OLIVE_CODEC_API int olive_audio_conform(const char* input_filename,
                                         const char* output_filename,
                                         OliveAudioParams target_params);

#ifdef __cplusplus
}
#endif

#endif  // OLIVE_CODEC_API_H
```

### 2.2 实现要点

```cpp
// c_api/src/codec_api.cpp

#include "olive/codec_api.h"
#include "codec/decoder.h"
#include "codec/ffmpeg/ffmpegdecoder.h"
#include "codec/frame.h"
#include "codec/encoder.h"
#include "codec/ffmpeg/ffmpegencoder.h"
#include "common/ffmpegutils.h"

struct OliveDecoder {
    olive::DecoderPtr impl;
};

struct OliveFrame {
    olive::FramePtr impl;
};

// ... 其他不透明指针定义 ...

extern "C" {

OliveDecoder* olive_decoder_create(const char* codec_id) {
    try {
        auto* d = new OliveDecoder();
        // 根据 codec_id 创建对应的解码器实例
        // 若 codec_id 为 nullptr 或 "auto"，则自动探测
        d->impl = olive::Decoder::CreateFromID(QString::fromUtf8(codec_id));
        return d;
    } catch (...) {
        return nullptr;
    }
}

void olive_decoder_destroy(OliveDecoder* decoder) {
    delete decoder;
}

int olive_decoder_open(OliveDecoder* decoder, const char* filename, int stream_index) {
    if (!decoder || !filename) return OLIVE_ERROR_INVALID;
    try {
        bool ok = decoder->impl->Open(QString::fromUtf8(filename), stream_index);
        return ok ? OLIVE_OK : OLIVE_ERROR_GENERIC;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

int olive_decoder_decode_video(OliveDecoder* decoder, OliveRational time, OliveFrame** out_frame) {
    if (!decoder || !out_frame) return OLIVE_ERROR_INVALID;
    try {
        olive::Rational t(time.num, time.den);
        olive::FramePtr frame = decoder->impl->RetrieveVideo(t);
        if (!frame) return OLIVE_ERROR_NOT_FOUND;
        auto* f = new OliveFrame();
        f->impl = frame;
        *out_frame = f;
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

// ... 其他函数类似封装 ...

}  // extern "C"
```

---

## 3. CMake 改造

### 3.1 `app/codec/CMakeLists.txt`

```cmake
# 收集 codec/ 内部源文件
set(CODEC_INTERNAL_SOURCES
  decoder.cpp decoder.h
  encoder.cpp encoder.h
  frame.cpp frame.h
  stream.cpp stream.h
  conformmanager.cpp conformmanager.h
  ffmpeg/ffmpegdecoder.cpp ffmpeg/ffmpegdecoder.h
  ffmpeg/ffmpegencoder.cpp ffmpeg/ffmpegencoder.h
  oiio/oiiodecoder.cpp oiio/oiiodecoder.h
  oiio/oiioencoder.cpp oiio/oiioencoder.h
  # ...
)

# 收集 common/ 源文件（并入 codec 库）
set(COMMON_INTERNAL_SOURCES
  ../common/ffmpegutils.cpp ../common/ffmpegutils.h
  ../common/xmlutils.cpp ../common/xmlutils.h
  ../common/filefunctions.cpp ../common/filefunctions.h
  ../common/qtutils.cpp ../common/qtutils.h
  ../common/debug.cpp ../common/debug.h
  # ...
)

# C API 封装层
set(CODEC_API_SOURCES
  ${CMAKE_SOURCE_DIR}/c_api/src/codec_api.cpp
)

add_library(olivecodec SHARED
  ${CODEC_INTERNAL_SOURCES}
  ${COMMON_INTERNAL_SOURCES}
  ${CODEC_API_SOURCES}
)

target_compile_definitions(olivecodec PRIVATE OLIVE_BUILDING_CODEC)

target_include_directories(olivecodec
  PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/c_api/include
  PUBLIC
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(olivecodec
  PUBLIC
    olivecore
    FFMPEG::avcodec
    FFMPEG::avformat
    FFMPEG::avutil
    FFMPEG::swscale
    FFMPEG::swresample
    FFMPEG::avfilter
    ${OIIO_LIBRARIES}
    ${OPENEXR_LIBRARIES}
)

set_target_properties(olivecodec PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

install(TARGETS olivecodec DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/c_api/include/olive/codec_api.h DESTINATION include/olive)
```

---

## 4. 小步快跑实施步骤

### Step 0: 分析依赖关系（半天）

- [ ] 梳理 `app/codec/` 和 `app/common/` 中的所有文件。
- [ ] 确认 `common/` 中不包含任何 Qt GUI 相关代码（若有，移出到 `liboliveui.so`）。
- [ ] 列出 `codec/` 和 `common/` 对 `node/` 的反向依赖（理论上不应有，若有需先解耦）。

**验收标准**：确认 `codec/` + `common/` 的依赖图只包含 `ext/core/`、FFmpeg、OIIO、Qt::Core。

### Step 1: 合并 common 到 codec 库（1 天）

- [ ] 修改 `app/codec/CMakeLists.txt`，将 `app/common/` 的源文件并入。
- [ ] 将 `app/common/CMakeLists.txt` 改为空文件（或删除，保留 add_subdirectory 空壳以兼容）。
- [ ] 确保编译产物为 `libolivecodec.so`（或 `.dylib`/`.dll`）。

**验收标准**：`libolivecodec.so` 编译成功，原有单元测试通过。

### Step 2: 设计 C API 的最小子集（1 天）

- [ ] 先只实现渲染流程**最必需**的接口：
  - `olive_decoder_create/open/destroy`
  - `olive_decoder_decode_video`
  - `olive_frame_width/height/data/destroy`
  - `olive_media_info_probe`
- [ ] 暂不实现：Encoder、Conform、音频解码的复杂场景。

**验收标准**：可以用 C API 打开一个视频文件并解码出一帧。

### Step 3: 编写 C API 实现（2 天）

- [ ] 编写 `c_api/include/olive/codec_api.h`（最小子集）。
- [ ] 编写 `c_api/src/codec_api.cpp`。
- [ ] 每个函数用 `try/catch(...)` 包裹，异常转换为 `OLIVE_ERROR_GENERIC`。
- [ ] 在 `c_api/tests/test_codec_api.cpp` 中编写测试。

**验收标准**：
```cpp
OliveDecoder* d = olive_decoder_create(nullptr);
olive_decoder_open(d, "test.mp4", 0);
OliveFrame* f = nullptr;
olive_decoder_decode_video(d, olive_rational_make(0, 1), &f);
assert(f != nullptr);
assert(olive_frame_width(f) > 0);
olive_frame_destroy(f);
olive_decoder_destroy(d);
```

### Step 4: 显式加载验证（1 天）

- [ ] 在主进程中通过 `ModuleLoader` 加载 `libolivecodec.so`。
- [ ] 验证可以成功解码测试视频并显示帧尺寸。

**验收标准**：主进程日志输出成功加载 `codec`，并能获取测试视频的宽和高。

### Step 5: 扩展 C API（按需迭代）

- [ ] 根据 `node/` 和 `render/` 的需要，逐步增加 Encoder、音频解码、Conform 等接口。
- [ ] 每次增加后运行编解码单元测试。

---

## 5. 风险与回退

| 风险 | 对策 |
|---|---|
| `FramePtr` 是 `std::shared_ptr`，C API 中需要管理引用计数 | `OliveFrame` 不透明指针内部持有 `std::shared_ptr`，销毁时自动减引用计数。若需要延长生命周期，可新增 `olive_frame_ref/unref`。 |
| `Decoder::Open` 是异步/多线程的 | C API 层面先做同步封装（等待 Open 完成）。若性能不满足，后续可新增异步回调接口。 |
| `common/` 中的 `xmlutils.h` 依赖 Qt XML | 这是允许的（Qt::Core 的一部分），但需注意 `common/` 中若混入 GUI 相关代码（如 `QMessageBox`），必须移出。 |
| FFmpeg 的 `AVFrame` 到 `olive::Frame` 转换在 C API 边界 | 保持内部实现不变，C API 只操作 `olive::Frame`。 |
