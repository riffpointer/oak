# oak_frame_api.h — 跨模块零拷贝帧抽象

> 这是 `oakcodec.so`、`oakgl.so`、`oakengine.so` 之间的**共享帧描述符**。  
> 它既不包含 FFmpeg 头文件，也不包含 OpenGL / Metal / Vulkan 头文件。  
> 任何模块只要 `#include "oak/frame_api.h"` 就能操作帧，而不需要知道帧内部是 `AVFrame`、OpenGL Texture 还是 CVPixelBuffer。
>
> **核心精度约定**：中间渲染链路（解码器输出 → 节点图 → 渲染器 → 编码器输入）**默认且强制使用 RGBA32F**。  
> 只有输入端（解码器从文件读取时）和输出端（编码器写入文件前）允许非 F32 格式，中间任何节点、模块、纹理、渲染目标都必须以 F32 处理。  
> 若某节点或插件不支持 F32，则在其输入端口由上游自动转换，输出端口再由其转回 F32。

---

## 一、设计目标

1. **零拷贝优先**：解码后的帧可以一直留在 GPU，直到渲染链末端或编码器需要时才回读。
2. **格式透明**：外部模块能看到帧的像素格式（RGBA8、YUV420P、NV12、P010 等），但不需要自己解释 planar 布局。
3. **生命周期统一**：无论帧在 CPU 还是 GPU，都通过 `oak_frame_release()` 释放，内部自动选择正确的析构路径。
4. **FFmpeg 不可见**：`internal` 字段对 codec 内部有意义（指向 `AVFrame`），但外部模块**绝对禁止**读取或 cast 它。

---

## 二、类型定义

```c
#ifndef OAK_FRAME_API_H
#define OAK_FRAME_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  帧存储位置                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_FRAME_CPU = 0,      /**< 数据在 CPU 内存，data[0..planes-1] 可读写 */
    OAK_FRAME_GPU,          /**< 数据在 GPU texture，data[0] 为 OakTextureHandle */
    OAK_FRAME_EXTERNAL,     /**< 外部平台 surface，data[0] 为平台句柄（CVPixelBufferRef / D3D11Texture / EGLImage 等） */
} OakFrameStorage;

/* ------------------------------------------------------------------ */
/*  像素格式（同时覆盖 RGBA 与 YUV/压缩格式）                              */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_FRAME_PIX_INVALID = 0,

    /* ---- RGBA packed ---- */
    /*
     * 中间渲染链路默认格式：RGBA32F。
     * 解码器输出到节点图、节点图内部传递、渲染器纹理与目标、编码器输入，
     * 全部优先使用 OAK_FRAME_PIX_RGBA32F。
     * OAK_FRAME_PIX_RGBA8 / RGBA16 仅在以下场景出现：
     *   - 缩略图生成（oak_decoder_thumbnail）
     *   - 最终编码器输出（编码前由 codec 内部转码）
     *   - 明确不支持 F32 的 OpenFX 插件节点（由 PluginNode 输入/输出自动转换）
     */
    OAK_FRAME_PIX_RGBA8,
    OAK_FRAME_PIX_RGBA16,
    OAK_FRAME_PIX_RGBA32F,

    /* ---- YUV 4:2:0 planar ---- */
    OAK_FRAME_PIX_YUV420P8,   /* 8-bit  Y/U/V 三分离 */
    OAK_FRAME_PIX_YUV420P10,  /* 10-bit Y/U/V 三分离（stride 以 16-bit 计算） */

    /* ---- YUV 4:2:2 planar ---- */
    OAK_FRAME_PIX_YUV422P8,
    OAK_FRAME_PIX_YUV422P10,

    /* ---- YUV 4:4:4 planar ---- */
    OAK_FRAME_PIX_YUV444P8,
    OAK_FRAME_PIX_YUV444P10,

    /* ---- semi-planar（NV 系列）---- */
    OAK_FRAME_PIX_NV12,       /* 8-bit  Y + UV interleaved */
    OAK_FRAME_PIX_NV21,       /* 8-bit  Y + VU interleaved（Android 常用） */
    OAK_FRAME_PIX_P010,       /* 10-bit Y + UV interleaved（HDR） */
    OAK_FRAME_PIX_P016,       /* 16-bit Y + UV interleaved */

    /* ---- 单/双通道（mask、depth、normal）---- */
    OAK_FRAME_PIX_R8,
    OAK_FRAME_PIX_RG8,
    OAK_FRAME_PIX_R16,
    OAK_FRAME_PIX_R32F,

    /* ---- 硬件加速 opaque 格式（仅用于 EXTERNAL）---- */
    OAK_FRAME_PIX_HW_VIDEOTOOLBOX,   /* macOS/iOS CVPixelBuffer */
    OAK_FRAME_PIX_HW_D3D11,          /* Windows D3D11Texture2D */
    OAK_FRAME_PIX_HW_VAAPI,          /* Linux VAAPI surface */
    OAK_FRAME_PIX_HW_CUDA,           /* NVIDIA CUDA array */
    OAK_FRAME_PIX_HW_MEDIACODEC,     /* Android MediaCodec surface */
} OakFramePixelFormat;

/* ------------------------------------------------------------------ */
/*  帧描述符（POD + 4 个指针）                                          */
/* ------------------------------------------------------------------ */

typedef struct OakFrame {
    int width;
    int height;
    OakFramePixelFormat pix_fmt;
    OakFrameStorage     storage;

    /* 时间戳（以流自己的时间基为单位） */
    int64_t pts_num;
    int64_t pts_den;

    /*
     * 色彩空间名称（OCIO 色彩空间标识符）。
     * 例如："ACES - ACEScg"、"Input - ARRI - V3 LogC (EI800) - Wide Gamut"、"Output - sRGB" 等。
     * 若为空字符串或 NULL，表示未标记，应由调用方按默认 IDT 处理。
     * 
     * 全链路 ACEScg 约定：
     *   - 解码器输出到节点图的帧：colorspace 应为 "ACES - ACEScg"
     *   - 节点图内部传递的帧：colorspace 应为 "ACES - ACEScg"
     *   - 渲染器 texture：colorspace 语义上为 "ACES - ACEScg"
     *   - 编码器输入的帧：colorspace 应为 "ACES - ACEScg"
     *   - 若某节点产生非 ACEScg 帧（如 PluginNode 输出 sRGB），colorspace 应标记真实空间，
     *     下游在需要时通过 oakcolor.so 转换回 ACEScg。
     */
    const char* colorspace;

    /*
     * 平面数据 / 句柄。
     *
     * CPU 模式：
     *   data[i]   = 第 i 个平面的首地址。
     *   stride[i] = 第 i 个平面的每行字节数（pitch）。
     *   planes    = 平面数量（RGBA=1, YUV420P=3, NV12=2 ...）。
     *
     * GPU 模式：
     *   若 pix_fmt 是 packed（RGBA8/16/32F）：
     *     data[0] = OakTextureHandle（void*），planes = 1。
     *   若 pix_fmt 是 planar（YUV420P、NV12 ...）：
     *     data[0..planes-1] = 每个平面对应的 OakTextureHandle。
     *     stride[i] 无意义，填 0。
     *
     * EXTERNAL 模式：
     *   data[0] = 平台相关句柄（如 CVPixelBufferRef）。
     *   planes / stride 由平台决定，外部不得解释。
     */
    void* data[4];
    int   stride[4];
    int   planes;

    /*
     * 内部 opaque 指针。
     * 对 codec 内部而言，这通常是 AVFrame* 或 AVFrame 的引用计数包装。
     * 外部模块**绝对禁止**读取、cast、修改此字段。
     */
    void* internal;
} OakFrame;

/* ------------------------------------------------------------------ */
/*  生命周期                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief 释放帧并回收所有关联资源。
 * @param frame 帧指针（允许 NULL）。
 *
 * 根据 storage 类型，内部会做不同的释放：
 * - CPU：释放内部 AVFrame 及其 buffer。
 * - GPU：通过运行时加载的 oakgl.so 接口销毁 texture。
 * - EXTERNAL：释放对平台 surface 的引用（如 CVBufferRelease），但**不销毁**底层 surface（所有权在外部）。
 *
 * @note 若帧是从 oak_renderer_readback_frame 获得的 CPU 回读帧，
 *       内部会通过 oak_renderer_free_readback 释放 buffer。
 */
void oak_frame_release(OakFrame* frame);

/**
 * @brief 仅释放 internal 引用，但不释放 data[] 中的外部资源。
 * @param frame 帧指针。
 * @note 用于 EXTERNAL 模式：当平台 surface 的生命周期由系统（如 VideoToolbox）管理时，
 *       codec 只需要释放自己对 AVFrame 的引用，而不销毁 surface。
 */
void oak_frame_release_internal_only(OakFrame* frame);

#ifdef __cplusplus
}
#endif

#endif /* OAK_FRAME_API_H */
```

---

## 三、精度约定与全链路 F32

### 3.1 默认中间格式

在整个 Oak 渲染管线中，**`OAK_FRAME_PIX_RGBA32F` 是默认且强制的中间格式**。具体规则如下：

| 链路位置 | 期望格式 | 说明 |
|----------|----------|------|
| **解码器 → 节点图** | RGBA32F | `oak_decoder_read_video` 默认输出 RGBA32F（GPU 或 CPU）。若源文件本身为 HDR（如 P010、RGBA32F EXR），直接保留；若源为 SDR 8-bit，解码器内部上采样为 F32。 |
| **节点图内部** | RGBA32F | 所有节点（Blur、Merge、ColorCorrection、Transform 等）的输入输出端口均以 RGBA32F 为契约。 |
| **渲染器纹理/目标** | RGBA32F | `oakgl.so` 创建的 texture 和 target 默认格式为 `OAK_RENDER_PIX_FMT_RGBA32F`。 |
| **编码器输入** | RGBA32F | `oak_encoder_write_video` 接收的 `OakFrame` 应为 RGBA32F，编码器内部在送入 FFmpeg 前转换为编码器所需的像素格式（如 YUV420P8、ProRes 422）。 |
| **缩略图/预览** | RGBA8 | `oak_decoder_thumbnail` 返回 RGBA8 CPU buffer，因为 UI 预览不需要 F32 精度。 |
| **OpenFX 插件节点** | 依插件而定 | `PluginNode` 根据 OFX 声明的 bit depth（`kOfxBitDepthByte` / `Short` / `Half` / `Float`）决定内部格式。若插件只支持 8-bit，则 `PluginNode` 的输入端口自动将 RGBA32F → RGBA8，输出端口再转回 RGBA32F。 |

### 3.2 强制转换规则

1. **上游非 F32 → 下游 F32**：若某节点因特殊原因输出非 F32 帧（如 `PluginNode` 输出 8-bit），其输出端口必须自动插入一个**隐式格式转换节点**（internal `FormatConvertNode`），在帧传递给下游前转为 RGBA32F。
2. **解码器自动上采样 + IDT**：解码器从文件读取 8-bit / 10-bit / 16-bit 帧时：
   - 像素值在 `ProcessFrameIntoBuffer`（CPU）或 shader（GPU）中自动映射到 `0.0f ~ 1.0f`（或 HDR 场景下的线性光强）。
   - 通过 `oakcolor.so` 的 IDT（Input Device Transform）将源色彩空间转换到 **ACEScg**。例如：
     - H.264/Rec.709 素材 → IDT: "Input - Sony - S-Log3 - S-Gamut3.Cine" 或 "Input - Rec.709"
     - ARRI Alexa ProRes → IDT: "Input - ARRI - V3 LogC (EI800) - Wide Gamut"
     - iPhone HDR HEVC → IDT: "Input - Apple - Apple Log"
   - `OakFrame::colorspace` 标记为 `"ACES - ACEScg"`。
3. **编码器自动下采样 + ODT**：编码器在 `WriteFrame` 时：
   - 输入帧应为 RGBA32F + ACEScg。
   - 通过 `oakcolor.so` 的 ODT（Output Device Transform）将 ACEScg 转换到目标编码空间。例如：
     - H.264/YouTube 输出 → ODT: "Output - Rec.709"
     - HDR10 输出 → ODT: "Output - Rec.2020 PQ"
     - 电影院 DCP → ODT: "Output - P3-DCI"
   - 再通过 `sws_scale` 或 GPU shader 完成格式转换（F32 → YUV420P8/10）。
4. **显示预览 View Transform**：预览窗口（Viewer）在显示前必须通过 `oak_display_transform_apply` 做 View Transform（RRT + ODT），将 ACEScg 转换到显示设备的色彩空间（sRGB、Rec.709、P3 等）。**禁止直接显示 ACEScg 线性数据**（否则会看起来发灰、过曝）。
5. **节点图内部色彩空间一致性**：任何节点输出非 ACEScg 帧时，`OakFrame::colorspace` 必须标记真实空间。下游节点在读取前应检查 `colorspace`，若不为 ACEScg，则通过 `oak_color_processor_apply`（或 GPU shader LUT）转换回 ACEScg。

### 3.3 为什么全链路 ACEScg + F32？

| 维度 | ACEScg + F32 的优势 |
|------|---------------------|
| **精度** | F32 避免多次混合/模糊/校正的量化损失和 banding。 |
| **HDR** | F32 天然容纳 10000+ nits 的 PQ/HLG 内容。ACEScg 的 AP1 色域覆盖所有物理显示设备。 |
| **VFX 标准** | ILM、Weta、Pixar 的标准工作空间。与 Nuke、DaVinci Resolve、Fusion 的管线兼容。 |
| **物理正确** | Scene-referred linear 下，加法、乘法、模糊在数学上是物理正确的。Gamma 空间下做合成是错误的。 |
| **统一入口** | 无论素材来自 ARRI、RED、Sony、iPhone、Drone，都通过 IDT 统一到 ACEScg，下游节点无需关心源设备。 |
| **显示灵活** | 同一份 ACEScg 数据可以通过不同 ODT 同时输出到 sRGB（YouTube）、Rec.2020 PQ（HDR TV）、P3-DCI（影院）。 |
| **GPU 友好** | 现代 GPU float texture 性能与 8-bit 几乎无差别。linear 空间下的 shader 更简单（无需 gamma decode/encode）。 |

---

## 四、使用模式

### 4.1 纯 CPU 管线（fallback）

```c
OakFrame frame = {0};
int ret = oak_decoder_read_video(dec, 0, time_num, time_den,
                                 NULL,   /* renderer_hint = NULL，强制 CPU */
                                 &frame);
if (ret == 0 && frame.storage == OAK_FRAME_CPU) {
    // frame.data[0] 指向 RGBA8 CPU buffer
    // frame.stride[0] 为每行字节数
    process_on_cpu(frame.data[0], frame.stride[0]);
    oak_frame_release(&frame);
}
```

### 4.2 GPU 零拷贝管线（推荐）

```c
OakRendererHandle renderer = oak_renderer_create("opengl", NULL);

OakFrame frame = {0};
int ret = oak_decoder_read_video(dec, 0, time_num, time_den,
                                 renderer,  /* 传入 renderer，codec 内部 upload */
                                 &frame);
if (ret == 0 && frame.storage == OAK_FRAME_GPU) {
    // frame.data[0] 可直接 cast 为 OakTextureHandle
    OakTextureHandle tex = (OakTextureHandle)frame.data[0];
    oak_renderer_draw_quad(renderer, mvp, tex, OAK_BLEND_OVER, NULL, NULL);
    oak_frame_release(&frame);   // 内部自动销毁 texture
}
```

### 4.3 硬件加速直通（VideoToolbox / D3D11 / VAAPI）

```c
OakFrame frame = {0};
int ret = oak_decoder_read_video(dec, 0, time_num, time_den,
                                 renderer, &frame);
if (ret == 0 && frame.storage == OAK_FRAME_EXTERNAL) {
    // frame.pix_fmt == OAK_FRAME_PIX_HW_VIDEOTOOLBOX
    // frame.data[0] == CVPixelBufferRef
    // 此时 oakcodec 已经通过 oak_texture_wrap_external 创建了 texture
    // frame.data[1] 为对应的 OakTextureHandle（由 codec 在 internal 中维护）
    // 外部只需按 GPU 模式渲染即可
}
```

---

## 五、平面格式说明

| 像素格式 | planes | data[0] | data[1] | data[2] | data[3] |
|----------|--------|---------|---------|---------|---------|
| `RGBA8/16/32F` | 1 | RGBA packed | — | — | — |
| `YUV420P8/10` | 3 | Y plane | U plane | V plane | — |
| `YUV422P8/10` | 3 | Y plane | U plane | V plane | — |
| `YUV444P8/10` | 3 | Y plane | U plane | V plane | — |
| `NV12` | 2 | Y plane | UV interleaved | — | — |
| `NV21` | 2 | Y plane | VU interleaved | — | — |
| `P010/P016` | 2 | Y plane | UV interleaved（10/16 bit） | — | — |

**注意**：GPU 模式下，`data[i]` 是 `OakTextureHandle`（即 `void*`）。  
对于 packed 格式，只有 `data[0]` 有意义；对于 planar 格式，`data[0..planes-1]` 分别对应每个平面的 texture handle。

---

## 六、与 `oak_codec_api.h` / `oak_renderer_api.h` 的关系

- `oak_frame_api.h` **不** include 任何其他 oak 头文件。它是整个系统的"公共基础"。
- `oak_codec_api.h` 在视频/编码函数中使用 `OakFrame*`。
- `oak_renderer_api.h` 在 upload/readback 函数中使用 `OakFrame*`。
- 外部模块（`oakengine`、`oaknodes`、`oakrenderer`）只需要 `#include "oak/frame_api.h"` 就能传递帧，完全不需要知道 FFmpeg 或 OpenGL。
