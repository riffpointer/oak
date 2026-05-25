/*
 *  oakgl.so / oak_renderer C API (v2)
 *  渲染后端抽象层。唯一知道底层图形 API 的模块。
 *  全链路默认：RGBA32F + ACEScg (AP1, scene-referred linear)。
 */

#ifndef OAK_RENDERER_API_H
#define OAK_RENDERER_API_H

#include <stddef.h>
#include "oak/frame_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakRenderer*  OakRendererHandle;
typedef struct OakTexture*   OakTextureHandle;
typedef struct OakTarget*    OakTargetHandle;
typedef struct OakShader*    OakShaderHandle;
typedef struct OakFontAtlas* OakFontAtlasHandle;

/* ------------------------------------------------------------------ */
/*  像素格式                                                            */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_RENDER_PIX_FMT_RGBA8 = 0,
    OAK_RENDER_PIX_FMT_RGBA16,
    OAK_RENDER_PIX_FMT_RGBA32F,    /* 默认中间格式，全链路 F32 + ACEScg */
    OAK_RENDER_PIX_FMT_R8,
    OAK_RENDER_PIX_FMT_RG8,
    OAK_RENDER_PIX_FMT_R16,
    OAK_RENDER_PIX_FMT_R32F,
    OAK_RENDER_PIX_FMT_R8_SNORM,
    OAK_RENDER_PIX_FMT_RG8_SNORM,
} OakRenderPixelFormat;

/* ------------------------------------------------------------------ */
/*  混合 / 过滤 / 环绕                                                  */
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
/*  v2 新增：纹理平面描述                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    int                   width;
    int                   height;
    OakRenderPixelFormat  pix_fmt;
    const void*           data;
    int                   stride;
} OakTexturePlane;

/* ------------------------------------------------------------------ */
/*  渲染器生命周期                                                      */
/* ------------------------------------------------------------------ */

OakRendererHandle oak_renderer_create(const char* backend_name, void* shared_context);
void              oak_renderer_destroy(OakRendererHandle renderer);
const char*       oak_renderer_backend_name(OakRendererHandle renderer);
int               oak_renderer_capability(OakRendererHandle renderer, const char* capability);

/* ------------------------------------------------------------------ */
/*  纹理管理                                                            */
/* ------------------------------------------------------------------ */

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

/* v2 新增：planar 上传 */
OakTextureHandle oak_texture_create_planar(OakRendererHandle renderer,
                                           int width, int height,
                                           OakTexturePlane* planes, int plane_count);

/* v2 新增：外部 surface 包装 */
OakTextureHandle oak_texture_wrap_external(OakRendererHandle renderer,
                                           int width, int height,
                                           OakRenderPixelFormat pix_fmt,
                                           void* external_handle,
                                           const char* external_type);

void oak_texture_destroy(OakRendererHandle renderer, OakTextureHandle texture);
void oak_texture_size(OakTextureHandle texture, int* out_width, int* out_height);

/* v2: texture download to CPU */
int oak_texture_download(OakRendererHandle renderer, OakTextureHandle texture,
                         int width, int height,
                         OakRenderPixelFormat pix_fmt,
                         void* out_data, int stride);

/* v2: single pixel query */
int oak_renderer_get_pixel(OakRendererHandle renderer, OakTextureHandle texture,
                           float x, float y, float* out_rgba);

/* ------------------------------------------------------------------ */
/*  渲染目标                                                            */
/* ------------------------------------------------------------------ */

OakTargetHandle oak_target_create(OakRendererHandle renderer,
                                  int width, int height,
                                  OakRenderPixelFormat pix_fmt,
                                  bool has_depth);
void            oak_target_destroy(OakRendererHandle renderer, OakTargetHandle target);
void            oak_target_resize(OakRendererHandle renderer, OakTargetHandle target,
                                  int width, int height);
void            oak_target_size(OakTargetHandle target, int* out_width, int* out_height);
OakTextureHandle oak_target_color_texture(OakRendererHandle renderer,
                                          OakTargetHandle target);
OakTextureHandle oak_target_detach_color_texture(OakRendererHandle renderer,
                                               OakTargetHandle target);

/* ------------------------------------------------------------------ */
/*  绘制指令                                                            */
/* ------------------------------------------------------------------ */

void oak_renderer_begin(OakRendererHandle renderer, OakTargetHandle target,
                        const float* clear_color);
void oak_renderer_end(OakRendererHandle renderer);
void oak_renderer_flush(OakRendererHandle renderer);
void oak_renderer_clear_texture(OakRendererHandle renderer, OakTextureHandle texture,
                                const float* clear_color);

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

/* v2 新增：显示变换（View Transform / ODT） */
void oak_renderer_apply_display_transform(OakRendererHandle renderer,
                                          OakTargetHandle source_target,
                                          OakTargetHandle dest_target,
                                          void* display_transform_handle);

/* v2 新增：GPU YUV→RGBA32F ACEScg */
void oak_renderer_blit_yuv_to_rgba(OakRendererHandle renderer,
                                   OakTextureHandle y_tex,
                                   OakTextureHandle u_tex,
                                   OakTextureHandle v_tex,
                                   OakTargetHandle dest_target,
                                   int width, int height,
                                   const float* color_matrix,
                                   bool full_range,
                                   OakFramePixelFormat pix_fmt);

/* ------------------------------------------------------------------ */
/*  回读                                                                */
/* ------------------------------------------------------------------ */

int  oak_renderer_readback(OakRendererHandle renderer, OakTargetHandle target,
                           OakRenderPixelFormat out_pix_fmt,
                           void** out_data, int* out_stride);
void oak_renderer_free_readback(void* data);

/* v2 新增：帧级回读 */
int oak_renderer_readback_frame(OakRendererHandle renderer,
                                void* source, int source_type,
                                OakFramePixelFormat out_pix_fmt,
                                OakFrame* out_frame);

/* ------------------------------------------------------------------ */
/*  Shader                                                              */
/* ------------------------------------------------------------------ */

OakShaderHandle oak_shader_compile(OakRendererHandle renderer,
                                   const char* shader_name,
                                   const char* vertex_source,
                                   const char* fragment_source);
void            oak_shader_destroy(OakRendererHandle renderer, OakShaderHandle shader);

/* --- Legacy JSON-based uniform passing (kept for compatibility) --- */
void oak_renderer_draw_with_shader(OakRendererHandle renderer,
                                   OakShaderHandle shader,
                                   const char* uniforms_json,
                                   OakTextureHandle* textures, int texture_count,
                                   OakTargetHandle dest_target);

void oak_renderer_draw_with_shader_to_texture(OakRendererHandle renderer,
                                              OakShaderHandle shader,
                                              const char* uniforms_json,
                                              OakTextureHandle* textures, int texture_count,
                                              OakTextureHandle dest_texture);

/* --- v2: Binary uniform descriptor (zero string overhead) --- */
typedef enum {
    OAK_UNIFORM_INT = 0,
    OAK_UNIFORM_FLOAT,
    OAK_UNIFORM_VEC2,
    OAK_UNIFORM_VEC3,
    OAK_UNIFORM_VEC4,
    OAK_UNIFORM_MAT4,
    OAK_UNIFORM_TEXTURE,
} OakUniformType;

typedef struct {
    const char* name;
    OakUniformType type;
    int texture_unit;   /* valid when type == OAK_UNIFORM_TEXTURE */
    union {
        int   i;
        float f;
        float vec2[2];
        float vec3[3];
        float vec4[4];
        float mat4[16];
    } value;
} OakShaderUniform;

void oak_renderer_draw_with_shader_ex(OakRendererHandle renderer,
                                      OakShaderHandle shader,
                                      const OakShaderUniform* uniforms,
                                      int uniform_count,
                                      OakTextureHandle* textures,
                                      int texture_count,
                                      OakTargetHandle dest_target);

void oak_renderer_draw_with_shader_to_texture_ex(OakRendererHandle renderer,
                                                 OakShaderHandle shader,
                                                 const OakShaderUniform* uniforms,
                                                 int uniform_count,
                                                 OakTextureHandle* textures,
                                                 int texture_count,
                                                 OakTextureHandle dest_texture);

/* ------------------------------------------------------------------ */
/*  字体图集                                                            */
/* ------------------------------------------------------------------ */

OakFontAtlasHandle oak_font_load(OakRendererHandle renderer,
                                 const char* font_path, float font_size);
void             oak_font_destroy(OakRendererHandle renderer, OakFontAtlasHandle font);

#ifdef __cplusplus
}
#endif

#endif /* OAK_RENDERER_API_H */
