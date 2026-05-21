/*
 *  oakgl.so / oak_renderer C API
 *  渲染后端抽象层。唯一知道底层图形 API 的模块。
 */

#ifndef OAK_RENDERER_API_H
#define OAK_RENDERER_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Opaque handles                                                      */
/* ------------------------------------------------------------------ */

typedef struct OakRenderer*  OakRendererHandle;
typedef struct OakTexture*   OakTextureHandle;
typedef struct OakTarget*    OakTargetHandle;
typedef struct OakShader*    OakShaderHandle;
typedef struct OakFontAtlas* OakFontAtlasHandle;

/* ------------------------------------------------------------------ */
/*  Enums                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_RENDER_PIX_FMT_RGBA8 = 0,
    OAK_RENDER_PIX_FMT_RGBA16,
    OAK_RENDER_PIX_FMT_RGBA32F,
    OAK_RENDER_PIX_FMT_R8,          /* single channel (mask, depth) */
    OAK_RENDER_PIX_FMT_RG8,         /* dual channel (normal map) */
} OakRenderPixelFormat;

typedef enum {
    OAK_BLEND_REPLACE = 0,   /* src */
    OAK_BLEND_OVER,          /* src over dst */
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
/*  Renderer lifecycle                                                  */
/* ------------------------------------------------------------------ */

OakRendererHandle oak_renderer_create(const char* backend_name, void* shared_context);
void              oak_renderer_destroy(OakRendererHandle renderer);
const char*       oak_renderer_backend_name(OakRendererHandle renderer);
int               oak_renderer_capability(OakRendererHandle renderer, const char* capability);

/* ------------------------------------------------------------------ */
/*  Texture management                                                  */
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

void oak_texture_destroy(OakRendererHandle renderer, OakTextureHandle texture);
void oak_texture_size(OakTextureHandle texture, int* out_width, int* out_height);

/* ------------------------------------------------------------------ */
/*  Render target (FBO / render pass)                                   */
/* ------------------------------------------------------------------ */

OakTargetHandle oak_target_create(OakRendererHandle renderer,
                                  int width, int height,
                                  OakRenderPixelFormat pix_fmt,
                                  bool has_depth);
void            oak_target_destroy(OakRendererHandle renderer, OakTargetHandle target);
void            oak_target_resize(OakRendererHandle renderer, OakTargetHandle target,
                                  int width, int height);
void            oak_target_size(OakTargetHandle target, int* out_width, int* out_height);

/* ------------------------------------------------------------------ */
/*  High-level draw commands                                            */
/* ------------------------------------------------------------------ */

void oak_renderer_begin(OakRendererHandle renderer, OakTargetHandle target,
                        const float* clear_color);
void oak_renderer_end(OakRendererHandle renderer);

/* ---- primitives ---- */
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

/* ---- post-processing ---- */
void oak_renderer_apply_effect(OakRendererHandle renderer,
                               const char* effect_name,
                               const char* params,
                               OakTargetHandle source_target,
                               OakTargetHandle dest_target);

/* ------------------------------------------------------------------ */
/*  Readback                                                            */
/* ------------------------------------------------------------------ */

int  oak_renderer_readback(OakRendererHandle renderer, OakTargetHandle target,
                           OakRenderPixelFormat out_pix_fmt,
                           void** out_data, int* out_stride);
void oak_renderer_free_readback(void* data);

/* ------------------------------------------------------------------ */
/*  Shader interface (advanced)                                         */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/*  Font atlas                                                          */
/* ------------------------------------------------------------------ */

OakFontAtlasHandle oak_font_load(OakRendererHandle renderer,
                                 const char* font_path, float font_size);
void             oak_font_destroy(OakRendererHandle renderer, OakFontAtlasHandle font);

#ifdef __cplusplus
}
#endif

#endif /* OAK_RENDERER_API_H */
