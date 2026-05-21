# oakgl.so / oak_renderer C API 设计

> 渲染后端抽象层。唯一知道底层图形 API（OpenGL / Vulkan / Metal / wgpu）的模块。
> 外部代码（包括 `oakrenderer` 进程）只调用本头文件中的函数，永不直接包含 OpenGL 头文件。

## 一、类型定义

```c
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakRenderer*  OakRendererHandle;
typedef struct OakTexture*   OakTextureHandle;
typedef struct OakTarget*    OakTargetHandle;
typedef struct OakShader*    OakShaderHandle;
typedef struct OakFontAtlas* OakFontAtlasHandle;

/**
 * @brief 像素格式（与 oakcodec.so 共用枚举，但在此处重新定义以避免头文件依赖）。
 */
typedef enum {
    OAK_RENDER_PIX_FMT_RGBA8 = 0,
    OAK_RENDER_PIX_FMT_RGBA16,
    OAK_RENDER_PIX_FMT_RGBA32F,
    OAK_RENDER_PIX_FMT_R8,          // 单通道（mask、depth）
    OAK_RENDER_PIX_FMT_RG8,         // 双通道（normal map）
} OakRenderPixelFormat;

/**
 * @brief 混合模式。
 */
typedef enum {
    OAK_BLEND_REPLACE = 0,   // src
    OAK_BLEND_OVER,          // src over dst（标准 alpha 混合）
    OAK_BLEND_ADD,           // 加法混合
    OAK_BLEND_MULTIPLY,      // 正片叠底
    OAK_BLEND_SCREEN,        // 滤色
    OAK_BLEND_SUBTRACT,      // 减法
} OakBlendMode;

/**
 * @brief 纹理过滤模式。
 */
typedef enum {
    OAK_FILTER_NEAREST = 0,
    OAK_FILTER_LINEAR,
    OAK_FILTER_MIPMAP_LINEAR,
} OakFilterMode;

/**
 * @brief 环绕模式。
 */
typedef enum {
    OAK_WRAP_CLAMP = 0,
    OAK_WRAP_REPEAT,
    OAK_WRAP_MIRROR,
} OakWrapMode;
```

## 二、渲染器生命周期

```c
/**
 * @brief 创建渲染器实例。
 * @param backend_name 后端名称字符串："opengl"、"vulkan"、"metal"、"wgpu"、"cpu"。
 * @param shared_context 可选的共享上下文指针（用于多窗口/多线程共享资源）。NULL 表示新建独立上下文。
 * @return 渲染器句柄，NULL 表示后端不可用或创建失败。
 */
OakRendererHandle oak_renderer_create(const char* backend_name, void* shared_context);

/**
 * @brief 销毁渲染器并释放所有关联资源（纹理、目标、shader 等）。
 * @param renderer 渲染器句柄。传 NULL 是安全的。
 */
void oak_renderer_destroy(OakRendererHandle renderer);

/**
 * @brief 获取当前后端名称。
 * @return 后端名称字符串常量（如 "opengl"）。
 */
const char* oak_renderer_backend_name(OakRendererHandle renderer);

/**
 * @brief 查询后端能力。
 * @param capability 能力名称："max_texture_size"、"supports_float_texture"、"supports_compute" 等。
 * @return 能力值（整数），若不支持返回 0。
 */
int oak_renderer_capability(OakRendererHandle renderer, const char* capability);
```

## 三、纹理管理

```c
/**
 * @brief 从 CPU 内存上传纹理。
 * @param renderer 渲染器句柄。
 * @param width 宽度（像素）。
 * @param height 高度（像素）。
 * @param pix_fmt 像素格式。
 * @param data 图像数据指针。
 * @param data_size 数据大小（字节）。
 * @param filter 过滤模式。
 * @param wrap 环绕模式。
 * @return 纹理句柄，NULL 表示失败。
 */
OakTextureHandle oak_texture_upload(OakRendererHandle renderer,
                                    int width, int height,
                                    OakRenderPixelFormat pix_fmt,
                                    const void* data, size_t data_size,
                                    OakFilterMode filter,
                                    OakWrapMode wrap);

/**
 * @brief 从视频帧数据创建纹理（零拷贝或快速路径）。
 * @param renderer 渲染器句柄。
 * @param width 宽度。
 * @param height 高度。
 * @param pix_fmt 像素格式。
 * @param data 数据指针。
 * @param stride 每行字节数（pitch）。
 * @return 纹理句柄。
 * @note 若像素格式与 GPU 原生格式不匹配，内部可能自动做 swizzle 或格式转换。
 */
OakTextureHandle oak_texture_upload_from_frame(OakRendererHandle renderer,
                                               int width, int height,
                                               OakRenderPixelFormat pix_fmt,
                                               const void* data, int stride);

/**
 * @brief 销毁纹理。
 */
void oak_texture_destroy(OakRendererHandle renderer, OakTextureHandle texture);

/**
 * @brief 查询纹理尺寸。
 */
void oak_texture_size(OakTextureHandle texture, int* out_width, int* out_height);
```

## 四、渲染目标（FBO / Render Pass）

```c
/**
 * @brief 创建离屏渲染目标。
 * @param renderer 渲染器句柄。
 * @param width 宽度。
 * @param height 高度。
 * @param pix_fmt 颜色缓冲区格式。
 * @param has_depth 是否附加深度/模板缓冲区。
 * @return 目标句柄。
 */
OakTargetHandle oak_target_create(OakRendererHandle renderer,
                                  int width, int height,
                                  OakRenderPixelFormat pix_fmt,
                                  bool has_depth);

void oak_target_destroy(OakRendererHandle renderer, OakTargetHandle target);

/**
 * @brief 调整目标尺寸（保留内容或清空）。
 */
void oak_target_resize(OakRendererHandle renderer, OakTargetHandle target,
                       int width, int height);

/**
 * @brief 查询目标尺寸。
 */
void oak_target_size(OakTargetHandle target, int* out_width, int* out_height);
```

## 五、高级绘制指令（与图形 API 无关）

所有绘制函数必须在 `oak_renderer_begin` / `oak_renderer_end` 对之间调用。

```c
/**
 * @brief 开始一帧渲染。
 * @param renderer 渲染器句柄。
 * @param target 渲染目标（NULL 表示默认屏幕/交换链）。
 * @param clear_color 清屏颜色 RGBA float[4]。传 NULL 表示不清屏。
 */
void oak_renderer_begin(OakRendererHandle renderer, OakTargetHandle target,
                        const float* clear_color);

/**
 * @brief 结束一帧渲染，提交所有指令。
 */
void oak_renderer_end(OakRendererHandle renderer);

/* ---- 基础图元 ---- */

/**
 * @brief 绘制一个带纹理的四边形（两个三角形）。
 * @param renderer 渲染器句柄。
 * @param mvp_matrix 4x4 模型-视图-投影矩阵（列主序 float[16]）。
 * @param texture 纹理句柄（NULL 表示纯色填充）。
 * @param blend_mode 混合模式。
 * @param color 顶点颜色/色调 RGBA float[4]（传 NULL 表示白色不透明）。
 * @param uv_rect 纹理坐标范围 float[4] = {u0, v0, u1, v1}。传 NULL 表示 {0,0,1,1}。
 */
void oak_renderer_draw_quad(OakRendererHandle renderer,
                            const float* mvp_matrix,
                            OakTextureHandle texture,
                            OakBlendMode blend_mode,
                            const float* color,
                            const float* uv_rect);

/**
 * @brief 绘制文字（使用内部字体图集）。
 * @param renderer 渲染器句柄。
 * @param utf8_text UTF-8 编码文本。
 * @param transform_matrix 4x4 变换矩阵。
 * @param font_size 字体大小（像素）。
 * @param color RGBA float[4]。
 */
void oak_renderer_draw_text(OakRendererHandle renderer,
                            const char* utf8_text,
                            const float* transform_matrix,
                            float font_size,
                            const float* color);

/* ---- 几何体 ---- */

/**
 * @brief 绘制线段列表。
 * @param points 顶点数组 float[N*2]（每点 x, y，在 NDC 空间 -1~1）。
 * @param point_count 顶点数。
 * @param color RGBA float[4]。
 * @param line_width 线宽（像素）。
 */
void oak_renderer_draw_lines(OakRendererHandle renderer,
                             const float* points, int point_count,
                             const float* color, float line_width);

/**
 * @brief 绘制多边形填充。
 * @param points 顶点数组 float[N*2]。
 * @param point_count 顶点数。
 * @param color RGBA float[4]。
 */
void oak_renderer_draw_polygon(OakRendererHandle renderer,
                               const float* points, int point_count,
                               const float* color);

/* ---- 后处理 / 效果 ---- */

/**
 * @brief 对当前目标应用一个简单的后处理效果。
 * @param renderer 渲染器句柄。
 * @param effect_name 效果名称："gaussian_blur"、"box_blur"、"gamma_correction"。
 * @param params 效果参数字符串（JSON 格式），如 '{"radius": 5.0}'。
 * @param source_target 输入目标（NULL 表示当前已绑定的目标）。
 * @param dest_target 输出目标（NULL 表示屏幕）。
 */
void oak_renderer_apply_effect(OakRendererHandle renderer,
                               const char* effect_name,
                               const char* params,
                               OakTargetHandle source_target,
                               OakTargetHandle dest_target);
```

## 六、数据回读

```c
/**
 * @brief 将渲染目标的内容回读到 CPU 内存。
 * @param renderer 渲染器句柄。
 * @param target 目标句柄（NULL 表示当前屏幕内容，但可能受平台限制）。
 * @param out_pix_fmt 请求的输出像素格式。
 * @param out_data 输出数据指针。由 oakgl.so 分配，调用者通过 oak_renderer_free_readback 释放。
 * @param out_stride 每行字节数。
 * @return 0 成功，非 0 失败。
 */
int oak_renderer_readback(OakRendererHandle renderer, OakTargetHandle target,
                          OakRenderPixelFormat out_pix_fmt,
                          void** out_data, int* out_stride);

void oak_renderer_free_readback(void* data);
```

## 七、Shader 接口（高级，供自定义节点使用）

```c
/**
 * @brief 从 GLSL / SPIR-V / Metal SL 源码编译 shader。
 * @param renderer 渲染器句柄。
 * @param shader_name 着色器名称（用于调试和缓存）。
 * @param vertex_source 顶点着色器源码（NULL 表示使用默认全屏 quad）。
 * @param fragment_source 片段着色器源码。
 * @return shader 句柄，NULL 表示编译失败。
 */
OakShaderHandle oak_shader_compile(OakRendererHandle renderer,
                                   const char* shader_name,
                                   const char* vertex_source,
                                   const char* fragment_source);

void oak_shader_destroy(OakRendererHandle renderer, OakShaderHandle shader);

/**
 * @brief 使用自定义 shader 绘制全屏 quad。
 * @param shader 自定义 shader 句柄。
 * @param uniforms_json uniform 参数字符串（JSON 格式），如 '{"uRadius": 5.0, "uTexture": 0}'。
 * @param textures 输入纹理数组。
 * @param texture_count 纹理数量。
 */
void oak_renderer_draw_with_shader(OakRendererHandle renderer,
                                   OakShaderHandle shader,
                                   const char* uniforms_json,
                                   OakTextureHandle* textures, int texture_count,
                                   OakTargetHandle dest_target);
```

## 八、字体图集（供文字渲染使用）

```c
/**
 * @brief 加载字体文件到图集。
 * @param renderer 渲染器句柄。
 * @param font_path 字体文件路径（TTF/OTF）。传 NULL 使用内置默认字体。
 * @param font_size 基础字号（像素）。
 * @return 字体图集句柄。
 */
OakFontAtlasHandle oak_font_load(OakRendererHandle renderer,
                                 const char* font_path, float font_size);

void oak_font_destroy(OakRendererHandle renderer, OakFontAtlasHandle font);
```

#ifdef __cplusplus
}
#endif
