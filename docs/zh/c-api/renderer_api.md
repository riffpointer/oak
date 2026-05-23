# oakgl.so / oak_renderer C API 设计（v2 — GPU 零拷贝扩展版）

> 渲染后端抽象层。唯一知道底层图形 API（OpenGL / Vulkan / Metal / wgpu）的模块。  
> v2 重点扩展：**planar texture 上传**、**GPU YUV→RGBA 转换**、**外部 surface 包装**、**帧回读统一接口**。  
> **v2.5 重点约定：全链路 F32 精度。** 渲染器默认使用 `RGBA32F` 格式，所有 texture、target、shader 输出均以 F32 处理。  
> 外部代码（包括 `oakrenderer` 进程）只调用本头文件中的函数，永不直接包含 OpenGL 头文件。

---

## 一、头文件关系

```c
#include "oak/frame_api.h"     /* OakFrame, OakFramePixelFormat ... */
#include "oak/renderer_api.h"  /* 本头文件 */
```

`oak/renderer_api.h` 内部会 `#include "oak/frame_api.h"`，因此调用者只需包含 `oak/renderer_api.h` 即可获得所有类型。

---

## 二、类型定义（v2 新增）

```c
#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakRenderer*  OakRendererHandle;
typedef struct OakTexture*   OakTextureHandle;
typedef struct OakTarget*    OakTargetHandle;
typedef struct OakShader*    OakShaderHandle;
typedef struct OakFontAtlas* OakFontAtlasHandle;

/* ------------------------------------------------------------------ */
/*  渲染器支持的像素格式（与 OakFramePixelFormat 子集对应）                */
/* ------------------------------------------------------------------ */

typedef enum {
    /*
     * 全链路 F32 + ACEScg 约定：
     * 中间渲染链路（texture、target、shader 输出）默认且强制使用 RGBA32F。
     * 所有 RGBA32F texture/target 的语义色彩空间为 ACEScg (AP1) linear。
     * RGBA8 仅在以下场景使用：
     *   - 最终屏幕显示（已通过 View Transform 从 ACEScg 转换到显示空间）
     *   - 缩略图 / 预览
     *   - 明确不支持 F32 的 OpenFX 插件内部
     */
    OAK_RENDER_PIX_FMT_RGBA8 = 0,   /* 仅在最终显示（已做 View Transform）或预览时使用 */
    OAK_RENDER_PIX_FMT_RGBA16,      /* 可选中间格式，用于带宽敏感场景（替代 32F） */
    OAK_RENDER_PIX_FMT_RGBA32F,     /* 默认中间格式，全链路 F32 + ACEScg */
    OAK_RENDER_PIX_FMT_R8,          /* 单通道（Y plane、mask） */
    OAK_RENDER_PIX_FMT_RG8,         /* 双通道（UV interleaved、normal map） */
    OAK_RENDER_PIX_FMT_R16,
    OAK_RENDER_PIX_FMT_R32F,
    OAK_RENDER_PIX_FMT_R8_SNORM,    /* 有符号归一化，用于 normal map */
    OAK_RENDER_PIX_FMT_RG8_SNORM,
} OakRenderPixelFormat;

/* ------------------------------------------------------------------ */
/*  混合 / 过滤 / 环绕（v1 保持不变）                                    */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_BLEND_REPLACE = 0,
    OAK_BLEND_OVER,
    OAK_BLEND_ADD,
    OAK_BLEND_MULTIPLY,
    OAK_BLEND_SCREEN,
    OAK_BLEND_SUBTRACT,
} OakBlendMode;

typedef enum {
    OAK_FILTER_NEAREST = 0,
    OAK_FILTER_LINEAR,
    OAK_FILTER_MIPMAP_LINEAR,
} OakFilterMode;

typedef enum {
    OAK_WRAP_CLAMP = 0,
    OAK_WRAP_REPEAT,
    OAK_WRAP_MIRROR,
} OakWrapMode;

/* ------------------------------------------------------------------ */
/*  v2 新增：纹理平面描述（用于 YUV planar 上传）                         */
/* ------------------------------------------------------------------ */

/**
 * @brief 单个纹理平面的描述。
 * @note width / height 可以小于帧尺寸（如 YUV420 的 U/V 平面为半宽半高）。
 */
typedef struct {
    int                   width;
    int                   height;
    OakRenderPixelFormat  pix_fmt;   /* 通常为 R8（Y）或 R8/RG8（UV） */
    const void*           data;      /* CPU 数据指针（GPU upload 时使用） */
    int                   stride;    /* 每行字节数 */
} OakTexturePlane;
```

---

## 三、渲染器生命周期（v1 保持不变）

```c
OakRendererHandle oak_renderer_create(const char* backend_name, void* shared_context);
void              oak_renderer_destroy(OakRendererHandle renderer);
const char*       oak_renderer_backend_name(OakRendererHandle renderer);
int               oak_renderer_capability(OakRendererHandle renderer, const char* capability);
```

**新增 capability 字符串**：

| capability | 返回值含义 |
|-----------|-----------|
| `"supports_external_texture"` | 是否支持 `oak_texture_wrap_external`（如 OpenGL ES 的 `EGLImage` 扩展）。 |
| `"supports_yuv_shader"` | 是否内置 YUV→RGBA32F ACEScg shader（所有 RGBA 后端必须支持）。 |
| `"max_texture_size"` | 最大纹理尺寸。 |
| `"supports_float_texture"` | 是否支持 float texture（RGBA32F / R32F）。OpenGL 3.0+ 必须返回 1。 |
| `"supports_compute"` | 是否支持 compute shader（用于未来 GPU 降噪/缩放）。 |
| `"supports_rgba32f_target"` | 是否支持 RGBA32F render target（FBO attachment）。 |

---

## 四、纹理管理（v2 重大扩展）

### 4.1 基础上传（v1 保留）

```c
OakTextureHandle oak_texture_upload(OakRendererHandle renderer,
                                    int width, int height,
                                    OakRenderPixelFormat pix_fmt,
                                    const void* data, size_t data_size,
                                    OakFilterMode filter,
                                    OakWrapMode wrap);

OakTextureHandle oak_texture_upload_from_frame(OakRendererHandle renderer,
                                               int width, int height,
                                               OakRenderPixelFormat pix_fmt,
                                               const void* data, int stride);

void oak_texture_destroy(OakRendererHandle renderer, OakTextureHandle texture);
void oak_texture_size(OakTextureHandle texture, int* out_width, int* out_height);
```

### 4.2 Planar 上传（v2 新增）

```c
/**
 * @brief 从多个平面创建一组纹理（用于 YUV 等 planar 格式）。
 * @param renderer     渲染器句柄。
 * @param width        帧宽度（像素）。
 * @param height       帧高度（像素）。
 * @param planes       平面描述数组。
 * @param plane_count  平面数量（1~4）。
 * @return 第一个平面的纹理句柄。
 *         内部保证返回的 texture handle 是连续的（如 plane 0 句柄值为 N，plane 1 为 N+1...），
 *         方便 caller 通过 data[0] + offset 索引。
 *         返回 NULL 表示失败。
 *
 * @note 每个平面独立为一张 texture。OpenGL 实现使用 `GL_TEXTURE_2D` + `GL_R8` / `GL_RG8`。
 *       Vulkan/Metal 实现使用 `R8Unorm` / `RG8Unorm`。
 */
OakTextureHandle oak_texture_create_planar(OakRendererHandle renderer,
                                           int width, int height,
                                           OakTexturePlane* planes, int plane_count);
```

### 4.3 外部 Surface 包装（v2 新增，零拷贝核心）

```c
/**
 * @brief 将平台外部 GPU surface 包装为 OakTextureHandle，零拷贝。
 * @param renderer       渲染器句柄。
 * @param width          宽度。
 * @param height         高度。
 * @param pix_fmt        像素格式（必须与外部 surface 一致）。
 * @param external_handle 平台句柄：
 *                        - macOS/iOS: `CVPixelBufferRef`（cast to void*）
 *                        - Windows:   `ID3D11Texture2D*`（cast to void*）
 *                        - Linux:     `VASurfaceID`（cast to uintptr_t 后强转 void*）
 *                        - Android:   `EGLImageKHR`（cast to void*）
 * @param external_type    平台类型字符串：
 *                        - `"videotoolbox"`
 *                        - `"d3d11"`
 *                        - `"vaapi"`
 *                        - `"eglimage"`
 *                        - `"cuda"`
 * @return 纹理句柄。失败返回 NULL。
 *
 * @note 包装后的 texture **不拥有**底层 surface 的生命周期。
 *       caller（通常是 codec）负责在适当的时候销毁 surface。
 *       但 texture 本身需要通过 oak_texture_destroy 释放，这会解除包装引用。
 */
OakTextureHandle oak_texture_wrap_external(OakRendererHandle renderer,
                                           int width, int height,
                                           OakRenderPixelFormat pix_fmt,
                                           void* external_handle,
                                           const char* external_type);
```

---

## 五、渲染目标（v1 保持不变）

```c
OakTargetHandle oak_target_create(OakRendererHandle renderer,
                                  int width, int height,
                                  OakRenderPixelFormat pix_fmt,
                                  bool has_depth);
void            oak_target_destroy(OakRendererHandle renderer, OakTargetHandle target);
void            oak_target_resize(OakRendererHandle renderer, OakTargetHandle target,
                                  int width, int height);
void            oak_target_size(OakTargetHandle target, int* out_width, int* out_height);
```

---

## 六、高级绘制指令（v2 新增 YUV→RGBA）

### 6.1 基础绘制（v1 保留）

```c
void oak_renderer_begin(OakRendererHandle renderer, OakTargetHandle target,
                        const float* clear_color);
void oak_renderer_end(OakRendererHandle renderer);

void oak_renderer_draw_quad(OakRendererHandle renderer,
                            const float* mvp_matrix,
                            OakTextureHandle texture,
                            OakBlendMode blend_mode,
                            const float* color,
                            const float* uv_rect);

void oak_renderer_draw_text(OakRendererHandle renderer,
                            const char* utf8_text,
                            const float* transform_matrix,
                            float font_size,
                            const float* color);

void oak_renderer_draw_lines(OakRendererHandle renderer,
                             const float* points, int point_count,
                             const float* color, float line_width);

void oak_renderer_draw_polygon(OakRendererHandle renderer,
                               const float* points, int point_count,
                               const float* color);

void oak_renderer_apply_effect(OakRendererHandle renderer,
                               const char* effect_name,
                               const char* params,
                               OakTargetHandle source_target,
                               OakTargetHandle dest_target);
```

### 6.2 GPU YUV→RGBA 转换（v2 新增）

```c
/**
 * @brief 使用内置 shader 将 YUV planar / semi-planar 纹理转换为 RGBA32F ACEScg，并输出到目标。
 * @param renderer    渲染器句柄。
 * @param y_tex       Y 平面纹理。
 * @param u_tex       U 平面纹理（NV12/NV21/P010 时传 UV interleaved 纹理）。
 * @param v_tex       V 平面纹理（NV12 等 semi-planar 格式时传 NULL）。
 * @param dest_target 输出目标（若为 NULL，则输出到当前绑定的 target）。
 * @param width       输出宽度。
 * @param height      输出高度。
 * @param color_matrix 可选的 3x4 色彩矩阵（float[12]），用于 YUV→RGB 转换和色彩空间映射。
 *                     传 NULL 时使用默认 BT.709 矩阵。
 *                     **注意**：此矩阵仅做 YUV→RGB 的线性转换，不处理色彩空间 primaries。
 *                     primaries 转换（如 Rec.709 → ACEScg）由 oakcolor.so 的 processor 在 upload 前或 blit 后处理。
 * @param full_range   是否为 full-range（0-255 映射到 0.0f~1.0f）。false 为 limited-range（16-235 映射到 0.0f~1.0f）。
 * @param pix_fmt      输入 YUV 子格式，用于决定采样方式：
 *                     - OAK_FRAME_PIX_YUV420P8 / YUV420P10
 *                     - OAK_FRAME_PIX_YUV422P8 / YUV422P10
 *                     - OAK_FRAME_PIX_YUV444P8 / YUV444P10
 *                     - OAK_FRAME_PIX_NV12 / NV21 / P010 / P016
 *
 * @note 对于 semi-planar 格式（NV12、P010），u_tex 为 UV interleaved 纹理，v_tex 必须为 NULL。
 * @note 对于 4:2:2 / 4:4:4，shader 内部会自动调整 UV 采样坐标（基于 width/height 和 chroma subsampling 比例）。
 * @note 输出目标格式**固定为 RGBA32F**，且语义上为 ACEScg linear。
 *       若 dest_target 的格式不是 RGBA32F，行为未定义。
 * @note 若输入 YUV 的色彩空间 primaries 与 ACEScg 不同（如 Rec.709 或 Rec.2020），
 *       caller 应在 blit 后通过 oakcolor.so 的 processor 做 primaries 转换，
 *       或在 shader 中通过额外的 uniform（如 `u_src_to_acesg_mat3`）一次性完成。
 */
void oak_renderer_blit_yuv_to_rgba(OakRendererHandle renderer,
                                   OakTextureHandle y_tex,
                                   OakTextureHandle u_tex,
                                   OakTextureHandle v_tex,
                                   OakTargetHandle dest_target,
                                   int width, int height,
                                   const float* color_matrix,
                                   bool full_range,
                                   OakFramePixelFormat pix_fmt);
```

### 6.3 使用自定义 Shader 绘制（v1 保留）

```c
OakShaderHandle oak_shader_compile(OakRendererHandle renderer,
                                   const char* shader_name,
                                   const char* vertex_source,
                                   const char* fragment_source);
void            oak_shader_destroy(OakRendererHandle renderer, OakShaderHandle shader);

void oak_renderer_draw_with_shader(OakRendererHandle renderer,
                                   OakShaderHandle shader,
                                   const char* uniforms_json,
                                   OakTextureHandle* textures, int texture_count,
                                   OakTargetHandle dest_target);
```

---

## 七、数据回读（v2 重大扩展）

### 7.1 基础回读（v1 保留）

```c
int  oak_renderer_readback(OakRendererHandle renderer, OakTargetHandle target,
                           OakRenderPixelFormat out_pix_fmt,
                           void** out_data, int* out_stride);
void oak_renderer_free_readback(void* data);
```

### 7.2 帧级回读（v2 新增）

```c
/**
 * @brief 将 GPU 帧（texture 或 target）回读到 CPU，包装为 OakFrame。
 * @param renderer     渲染器句柄。
 * @param source       源。可以是：
 *                     - OakTextureHandle（GPU texture）
 *                     - OakTargetHandle（render target）
 *                     - NULL（当前绑定的 target）
 * @param source_type  源类型：0 = texture, 1 = target, 2 = current target。
 * @param out_pix_fmt  请求的 CPU 像素格式。
 * @param out_frame    输出帧。调用者通过 oak_frame_release 释放。
 * @return 0 成功，非 0 失败。
 *
 * @note 回读后的 OakFrame 默认标记：
 *       - pix_fmt = out_pix_fmt（通常为 RGBA32F）
 *       - colorspace = "ACES - ACEScg"（因为 GPU texture 的语义空间就是 ACEScg）
 *
 * @note 这是编码器路径的关键函数：
 *       当编码器收到 OAK_FRAME_GPU 的输入帧时，内部调用此函数将 texture 回读到 CPU buffer，
 *       再送入 FFmpeg 编码器。回读格式固定为 RGBA32F ACEScg。
 *       若编码器支持硬件编码（如 VideoToolbox），可直接提交 external surface，绕过此回读。
 */
int oak_renderer_readback_frame(OakRendererHandle renderer,
                                void* source, int source_type,
                                OakFramePixelFormat out_pix_fmt,
                                OakFrame* out_frame);
```

---

## 八、字体图集（v1 保留）

```c
OakFontAtlasHandle oak_font_load(OakRendererHandle renderer,
                                 const char* font_path, float font_size);
void             oak_font_destroy(OakRendererHandle renderer, OakFontAtlasHandle font);
```

---

## 九、oakcodec.so 运行时加载符号清单

`oakcodec.so` 在第一次收到非 NULL 的 `renderer_hint` 时，会通过以下顺序加载 `oakgl.so`：

1. macOS: `dlopen("liboakgl.dylib", RTLD_NOW | RTLD_LOCAL)`
2. Linux: `dlopen("liboakgl.so", RTLD_NOW | RTLD_LOCAL)`
3. Windows: `LoadLibraryA("oakgl.dll")`

然后 `dlsym` 以下符号（全部来自 `oak/renderer_api.h`）：

| 符号名 | 用途 |
|--------|------|
| `oak_texture_upload_from_frame` | 上传 packed RGBA CPU buffer 到 GPU texture。 |
| `oak_texture_create_planar` | 上传 YUV planar CPU buffer 到多平面 GPU texture。 |
| `oak_texture_wrap_external` | 包装硬件解码器输出的外部 GPU surface。 |
| `oak_texture_destroy` | 释放 GPU texture。 |
| `oak_renderer_blit_yuv_to_rgba` | GPU YUV→RGBA 转换。 |
| `oak_renderer_readback_frame` | GPU→CPU 回读（编码器需要）。 |
| `oak_renderer_free_readback` | 释放回读 buffer。 |

**失败策略**：若 `dlopen` 或任一必需符号加载失败，`oakcodec.so` 必须静默回退到 CPU 路径（OAK_FRAME_CPU），绝不崩溃或报错（仅通过 `qDebug` 输出警告）。
