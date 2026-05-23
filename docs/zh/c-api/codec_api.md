# oakcodec.so C API 设计（v2 — GPU 零拷贝版）

> 媒体编解码：文件打开、视频/音频/字幕解码、编码输出、conform（音频预处理）。  
> **FFmpeg 调用被完全封装在 `oakcodec.so` 内部。外部模块不得直接 `#include <libavcodec/...>`。**  
> 视频帧通过 `OakFrame` 跨模块传递，支持 CPU buffer、GPU texture、外部硬件 surface 三种存储模式。

---

## 一、头文件关系

```c
#include "oak/frame_api.h"   /* OakFrame, OakFramePixelFormat, OakFrameStorage */
#include "oak/codec_api.h"   /* 本头文件 */
```

`oak/codec_api.h` 内部会 `#include "oak/frame_api.h"`，因此调用者只需包含 `oak/codec_api.h` 即可获得所有类型。

---

## 二、类型定义

```c
#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakDecoder*  OakDecoderHandle;
typedef struct OakEncoder*  OakEncoderHandle;
typedef struct OakConform*  OakConformHandle;

/* ------------------------------------------------------------------ */
/*  音频采样格式（编码器输入/解码器输出统一为交错 float32）                  */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_AUDIO_FMT_INVALID = 0,
    OAK_AUDIO_FMT_U8,
    OAK_AUDIO_FMT_S16,
    OAK_AUDIO_FMT_S32,
    OAK_AUDIO_FMT_FLT,
    OAK_AUDIO_FMT_DBL,
} OakAudioFormat;

/* ------------------------------------------------------------------ */
/*  流信息（POD，仅用于 probe）                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    int     width;
    int     height;
    OakFramePixelFormat pix_fmt;   /* 源自 frame_api.h */
    int64_t timebase_num;
    int64_t timebase_den;
    double  frame_rate;
    int64_t duration_frames;
} OakVideoStreamInfo;

typedef struct {
    int     sample_rate;
    int     channels;
    OakAudioFormat sample_fmt;
    int64_t timebase_num;
    int64_t timebase_den;
    int64_t duration_samples;
} OakAudioStreamInfo;

typedef struct {
    int video_stream_count;
    int audio_stream_count;
    int subtitle_stream_count;
    OakVideoStreamInfo* video_streams;
    OakAudioStreamInfo* audio_streams;
} OakMediaInfo;
```

---

## 三、解码器生命周期

```c
/**
 * @brief 打开媒体文件并创建解码器。
 * @param filepath   绝对或相对路径（UTF-8）。
 * @param codec_hint 可选的编解码器提示，如 "ffmpeg" / "oiio"。传 NULL 表示自动探测。
 * @param out_info   输出媒体信息。调用者负责用 oak_media_info_free 释放内部数组。
 * @return 解码器句柄，NULL 表示失败。
 */
OakDecoderHandle oak_decoder_open(const char* filepath, const char* codec_hint,
                                  OakMediaInfo* out_info);

void oak_decoder_close(OakDecoderHandle decoder);

void oak_media_info_free(OakMediaInfo* info);
```

---

## 四、视频解码（核心变更）

### 4.0 默认输出：RGBA32F + ACEScg

`oak_decoder_read_video` 的默认输出为：
- **像素格式**：`OAK_FRAME_PIX_RGBA32F`
- **色彩空间**：`"ACES - ACEScg"`（通过 OCIO IDT 转换）

这是全链路 F32 + ACEScg 的起点。

- 若源文件为 8-bit SDR（H.264/YUV420P8）：
  - 解码器通过 `sws_scale` 或 GPU shader 将 YUV → RGBA linear（值域映射到 `0.0f ~ 1.0f`）。
  - 通过 `oakcolor.so` 的 IDT 将源色彩空间（如 Rec.709、Rec.601、sRGB）转换到 ACEScg。
  - `OakFrame::colorspace` 标记为 `"ACES - ACEScg"`。
- 若源文件为 10-bit HDR（H.265/P010/HLG/PQ）：
  - 解码器将 PQ/HLG 的 non-linear 值域解曲线化为 scene-linear（通过 OCIO 或 FFmpeg 的 `zscale`）。
  - 通过 IDT 将源色彩空间（如 Rec.2020、P3-D65）转换到 ACEScg。
- 若源文件本身就是 float（OpenEXR RGBA32F）：
  - 若 EXR 的 metadata 声明了色彩空间（如 `chromaticities` 或 OCIO 属性），通过 IDT 转换到 ACEScg。
  - 若未声明，假设为 ACEScg 或 sRGB linear，再做转换。
- 若 caller 明确请求缩略图（`oak_decoder_thumbnail`）：
  - 返回 `OAK_FRAME_PIX_RGBA8`，且已通过 View Transform（RRT + sRGB ODT）转换到显示空间。
- 若 caller 明确请求保留源格式（通过未来扩展的 `out_pix_fmt` 参数）：
  - codec 内部完成转换，但 `colorspace` 仍标记为 ACEScg（因为像素值已经是 scene-linear AP1）。

**IDT 探测优先级**：
1. 文件元数据（ICC profile、QuickTime colr atom、MP4/VUI `colour_primaries`/`transfer_characteristics`）。
2. `FootageDescription` 中缓存的色彩空间（用户手动指定）。
3. 文件扩展名 + 编码器类型启发式（如 `.arri` → ARRI LogC Wide Gamut）。
4. 默认回退：`"Input - Rec.709"`（SDR）或 `"Input - Rec.2020"`（HDR）。

### 4.1 `oak_decoder_read_video`

```c
/**
 * @brief 读取指定时间点的视频帧。
 * @param decoder       解码器句柄。
 * @param stream_index  视频流索引（0-based）。
 * @param time_num      目标时间分子（以流时间基为单位）。
 * @param time_den      目标时间分母。
 * @param renderer_hint 可选的渲染器句柄（OakRendererHandle 强转 void*）。
 *                      - NULL：强制返回 CPU buffer（codec 内部做 swscale）。
 *                      - 非 NULL：codec 尝试在内部将帧 upload 到 GPU，返回 OAK_FRAME_GPU 或 OAK_FRAME_EXTERNAL。
 * @param out_frame     输出帧描述符。调用者通过 oak_frame_release 释放。
 * @return  0  成功
 *          1  需要更多数据（流未解码到该位置，调用者应稍后重试）
 *         -1  错误（EOF、解码失败、不支持格式）
 *
 * @note 零拷贝行为：
 *   1. 若解码器本身支持硬件加速（VideoToolbox / D3D11 / VAAPI / CUDA / MediaCodec），
 *      且 renderer_hint 非 NULL，codec 会直接返回 OAK_FRAME_EXTERNAL，不经过 CPU。
 *   2. 若解码器输出软件 YUV 帧，且 renderer_hint 非 NULL：
 *      - codec 在内部 dlopen liboakgl.dylib，调用 oak_texture_create_planar 上传 YUV 平面；
 *      - codec **默认**调用 oak_renderer_blit_yuv_to_rgba 在 GPU 上完成 YUV→RGBA32F 转换，
 *        最终返回一个 RGBA32F GPU texture（OAK_FRAME_GPU）；
 *      - 若 caller 明确要求保留 YUV（如后续节点需要原始 YUV 做特殊处理），则直接返回 YUV planar GPU texture。
 *   3. 若 renderer_hint 为 NULL，codec 回退到 CPU swscale，返回 OAK_FRAME_CPU。
 *      此时 swscale 的目标格式固定为 `AV_PIX_FMT_RGBAF32LE`，保证 CPU 路径也是 F32。
 * @note 全链路 F32 + ACEScg：无论走 GPU 还是 CPU 路径，解码器输出的默认像素格式为 RGBA32F，
 *       默认色彩空间为 "ACES - ACEScg"。只有缩略图和明确指定的非 F32/非 ACEScg 请求才会例外。
 *       OakFrame::colorspace 必须被正确设置，否则下游节点可能做错误的色彩空间转换。
 *
 * @note 缩放行为：
 *   out_frame->width / out_frame->height 由 codec 根据流原始分辨率和 caller 的期望决定。
 *   若 codec 内部做了缩放，硬件加速路径优先在 GPU 上做（如通过 shader 或 texture sampler），
 *   软件路径通过 swscale。
 */
int oak_decoder_read_video(OakDecoderHandle decoder, int stream_index,
                           int64_t time_num, int64_t time_den,
                           void* renderer_hint,
                           OakFrame* out_frame);
```

### 4.2 `oak_decoder_thumbnail`

```c
/**
 * @brief 获取视频帧缩略图（快速预览）。
 * @param max_size  缩略图最大边长（保持宽高比）。
 * @param out_frame 输出帧。缩略图总是 CPU buffer（OAK_FRAME_CPU），方便 UI 直接绘制。
 * @return 0 成功，非 0 失败。
 */
int oak_decoder_thumbnail(OakDecoderHandle decoder, int stream_index,
                          int max_size,
                          OakFrame* out_frame);
```

### 4.3 旧接口废弃声明

以下 v1 接口已废弃，将在 P3 阶段移除：

```c
/* 废弃：v1 CPU-only 接口 */
__attribute__((deprecated("use oak_decoder_read_video with OakFrame")))
int  oak_decoder_read_video_cpu(OakDecoderHandle decoder, int stream_index,
                                int64_t time_num, int64_t time_den,
                                void** out_data, int* out_stride);
__attribute__((deprecated("use oak_frame_release")))
void oak_frame_free(void* data);
```

---

## 五、音频解码与 Conform

音频不涉及 GPU，保持 v1 设计不变：

```c
/**
 * @brief 读取指定时间范围的音频采样（交错 float32）。
 * @return 0 成功，非 0 失败。
 */
int oak_decoder_read_audio(OakDecoderHandle decoder, int stream_index,
                           int64_t start_sample, int64_t sample_count,
                           float** out_data, int64_t* out_actual_samples);

void oak_audio_buffer_free(float* data);

/* ---- Conform（音频预处理缓存）---- */
int  oak_conform_get(const char* decoder_id, const char* cache_path,
                     int stream_index,
                     int target_sample_rate, int target_channels,
                     bool wait,
                     const char*** out_filenames, int* out_count);
int  oak_conform_poll(const char* decoder_id);
void oak_conform_free_filenames(const char** filenames, int count);
```

---

## 六、编码器（核心变更）

编码器输入也改为 `OakFrame*`，从而支持：
- 从 GPU texture 直接编码（内部 readback，无需 caller 操心）；
- 从 CPU buffer 编码（传统路径）；
- 从外部硬件 surface 编码（如通过 VideoToolbox 的硬件编码器直通）。

```c
OakEncoderHandle oak_encoder_create(const char* filepath,
                                    const char* container_format,
                                    const char* video_codec,
                                    const char* audio_codec);

void oak_encoder_close(OakEncoderHandle encoder);

void oak_encoder_set_video_params(OakEncoderHandle encoder,
                                  int width, int height,
                                  OakFramePixelFormat pix_fmt,
                                  int64_t timebase_num, int64_t timebase_den,
                                  double frame_rate);

/**
 * @brief 配置编码器视频输出格式（下采样目标格式）。
 * @param encoder     编码器句柄。
 * @param output_pix_fmt  编码器最终写入文件的像素格式，如 OAK_FRAME_PIX_YUV420P8、OAK_FRAME_PIX_YUV422P10。
 *                        默认值为 OAK_FRAME_PIX_YUV420P8。
 * @note 编码器输入始终期望 RGBA32F + ACEScg，此参数仅决定编码前的像素格式转换。
 */
void oak_encoder_set_video_output_format(OakEncoderHandle encoder,
                                         OakFramePixelFormat output_pix_fmt);

/**
 * @brief 配置编码器输出色彩空间（ODT 目标）。
 * @param encoder         编码器句柄。
 * @param output_colorspace  目标色彩空间名称（OCIO colorspace 字符串）。
 *                         如 "Output - Rec.709"、"Output - Rec.2020 PQ"、"Output - P3-DCI"、"Output - sRGB" 等。
 *                         默认值为 "Output - Rec.709"（SDR）或 "Output - Rec.2020 PQ"（若 frame_rate > 30 或 pix_fmt 为 HDR）。
 * @note 编码器在写入前会先通过 oakcolor.so 的 ODT 将 ACEScg 转换到此目标空间，再做格式转换。
 */
void oak_encoder_set_video_output_colorspace(OakEncoderHandle encoder,
                                             const char* output_colorspace);

void oak_encoder_set_audio_params(OakEncoderHandle encoder,
                                  int sample_rate, int channels,
                                  OakAudioFormat sample_fmt,
                                  int64_t timebase_num, int64_t timebase_den);

/**
 * @brief 写入一帧视频。
 * @param encoder 编码器句柄。
 * @param frame   输入帧。可以是 CPU、GPU 或 EXTERNAL。
 *                - **输入格式期望**：默认且强制为 `OAK_FRAME_PIX_RGBA32F` + `"ACES - ACEScg"`。
 *                  若传入非 F32 帧，编码器内部会先转换为 RGBA32F。
 *                  若 `frame->colorspace` 不为 `"ACES - ACEScg"`，编码器会先通过 `oakcolor.so` 转换到 ACEScg，
 *                  再通过 ODT 转换到目标输出空间。
 *                - CPU：codec 直接读取 data[0]（应为 RGBA32F ACEScg buffer）。
 *                - GPU / EXTERNAL：codec 内部通过 dlopen oakgl 调用 oak_renderer_readback_frame
 *                  将数据回读到 CPU（回读格式为 RGBA32F ACEScg），再送入编码器。
 *                  若编码器支持硬件编码（如 VideoToolbox），且 frame 为 EXTERNAL/CVPixelBuffer，
 *                  codec 可直接提交 surface，完全避免 CPU 回读。
 *                - 编码流程：
 *                  1. 确认 frame 为 RGBA32F + ACEScg（若不是，先做转换）。
 *                  2. 通过 ODT 将 ACEScg → output_colorspace（如 Rec.709 PQ）。
 *                  3. 通过 swscale / GPU shader 将 RGBA32F → output_pix_fmt（如 YUV420P10）。
 *                  4. 送入 FFmpeg 编码器。
 * @return 0 成功，非 0 失败。
 */
int oak_encoder_write_video(OakEncoderHandle encoder, const OakFrame* frame);

int oak_encoder_write_audio(OakEncoderHandle encoder,
                            const float* data, int64_t samples,
                            int64_t pts_num, int64_t pts_den);

int oak_encoder_finalize(OakEncoderHandle encoder);
```

---

## 七、FFmpeg 封装边界（强制规定）

| 规则 | 说明 |
|------|------|
| **唯一合法入口** | 只有 `oakcodec.so` 内部可以 `#include <libavcodec/avcodec.h>` 等 FFmpeg 头文件。 |
| **外部禁止** | `oakengine.so`、`oaknodes.so`、`oakgl.so`、`oakrenderer` 进程**绝对禁止**直接包含或使用任何 FFmpeg 类型（`AVFrame`、`AVCodecContext`、`SwsContext` 等）。 |
| **违规检测** | CI 脚本通过 `grep -r "libavcodec\|libavformat\|libavutil\|libswscale\|libswresample" --include="*.h" --include="*.cpp" engine/src/ app/src/ src/oakgl/ src/oakengine/` 扫描，若发现非 `src/oakcodec/` 目录下的引用，构建失败。 |
| **传递依赖** | `oakshared`（静态库）允许包含 `olive/common/ffmpegutils.h` 中的工具函数（如 `CreateAVFramePtr`），但这些函数不得被外部模块直接调用。仅供 `oakcodec` 内部使用。 |

---

## 八、零拷贝决策流程

```
oak_decoder_read_video(renderer_hint)
│
├─ renderer_hint == NULL ?
│   └─ YES → 软件解码 → swscale → CPU RGBA → OAK_FRAME_CPU
│
└─ renderer_hint != NULL ?
    ├─ 硬件解码器可用（VideoToolbox / VAAPI / D3D11 / CUDA）?
    │   └─ YES → 返回原始 GPU surface → OAK_FRAME_EXTERNAL
    │           (codec 通过 dlopen oakgl 调用 oak_texture_wrap_external 创建 texture)
    │
    └─ 软件解码 → YUV CPU buffer
        ├─ caller 请求 RGBA ?
        │   └─ YES → codec 内部 upload YUV plane → oakgl shader YUV→RGBA
        │           → 返回 OAK_FRAME_GPU (data[0] = RGBA texture)
        │
        └─ caller 请求 YUV ?
            └─ YES → codec 内部 upload YUV plane → 返回 OAK_FRAME_GPU (data[0..2] = Y/U/V textures)
```

**关键点**：即使 renderer_hint 非 NULL，如果 `dlopen("liboakgl.dylib")` 失败，codec 必须优雅回退到 CPU 路径，不崩溃。
