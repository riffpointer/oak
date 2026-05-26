/*
 *  oakgl.so / oak_renderer C API (v2)
 *  Render backend abstraction layer. The only module that knows the underlying
 *  graphics API (OpenGL / Vulkan / Metal / wgpu).
 *
 *  Full-pipeline default: RGBA32F + ACEScg (AP1, scene-referred linear).
 *  External code (including the oakrenderer process) must NEVER directly include
 *  OpenGL headers; all interaction goes through this C API.
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
/*  Pixel formats                                                       */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_RENDER_PIX_FMT_RGBA8 = 0,   /**< Final display / thumbnail only (after View Transform). */
    OAK_RENDER_PIX_FMT_RGBA16,      /**< Optional intermediate for bandwidth-sensitive scenes. */
    OAK_RENDER_PIX_FMT_RGBA32F,     /**< Default intermediate: full F32 + ACEScg. */
    OAK_RENDER_PIX_FMT_R8,          /**< Single channel (Y plane, mask). */
    OAK_RENDER_PIX_FMT_RG8,         /**< Dual channel (UV interleaved, normal map). */
    OAK_RENDER_PIX_FMT_R16,
    OAK_RENDER_PIX_FMT_R32F,
    OAK_RENDER_PIX_FMT_R8_SNORM,    /**< Signed normalized, for normal maps. */
    OAK_RENDER_PIX_FMT_RG8_SNORM,
} OakRenderPixelFormat;

/* ------------------------------------------------------------------ */
/*  Blend / filter / wrap modes                                         */
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
/*  Texture plane descriptor (v2)                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    int                   width;
    int                   height;
    OakRenderPixelFormat  pix_fmt;
    const void*           data;
    int                   stride;
} OakTexturePlane;

/* ------------------------------------------------------------------ */
/*  Renderer lifecycle                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a renderer backend.
 * @param backend_name    Backend name. Currently only "opengl" is supported.
 * @param shared_context  Optional shared context pointer (platform-specific).
 * @return Renderer handle, or NULL on failure (e.g. no GPU/display available).
 */
OakRendererHandle oak_renderer_create(const char* backend_name, void* shared_context);

/**
 * @brief Destroy a renderer and free all GPU resources.
 * @param renderer Renderer handle (NULL is silently ignored).
 */
void              oak_renderer_destroy(OakRendererHandle renderer);

/**
 * @brief Get the backend name string.
 * @param renderer Renderer handle.
 * @return Backend name (constant pointer, do not free).
 */
const char*       oak_renderer_backend_name(OakRendererHandle renderer);

/**
 * @brief Query a renderer capability.
 * @param renderer   Renderer handle.
 * @param capability Capability name string, e.g. "max_texture_size".
 * @return Capability value (positive integer), or 0 if unknown / invalid.
 */
int               oak_renderer_capability(OakRendererHandle renderer, const char* capability);

/* ------------------------------------------------------------------ */
/*  Texture management                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Upload pixel data to a GPU texture.
 * @param renderer  Renderer handle.
 * @param width     Texture width.
 * @param height    Texture height.
 * @param pix_fmt   Pixel format.
 * @param data      Raw pixel data pointer.
 * @param data_size Total data size in bytes.
 * @param filter    Min/mag filter mode.
 * @param wrap      Wrap mode.
 * @return Texture handle, or NULL on failure.
 */
OakTextureHandle oak_texture_upload(OakRendererHandle renderer,
                                    int width, int height,
                                    OakRenderPixelFormat pix_fmt,
                                    const void* data, size_t data_size,
                                    OakFilterMode filter,
                                    OakWrapMode wrap);

/**
 * @brief Upload pixel data from a frame buffer with explicit stride.
 * @param renderer  Renderer handle.
 * @param width     Texture width.
 * @param height    Texture height.
 * @param pix_fmt   Pixel format.
 * @param data      Raw pixel data pointer.
 * @param stride    Line stride in bytes.
 * @return Texture handle, or NULL on failure.
 */
OakTextureHandle oak_texture_upload_from_frame(OakRendererHandle renderer,
                                               int width, int height,
                                               OakRenderPixelFormat pix_fmt,
                                               const void* data, int stride);

/**
 * @brief Create a planar texture from multiple planes (v2).
 * @param renderer     Renderer handle.
 * @param width        Texture width.
 * @param height       Texture height.
 * @param planes       Array of plane descriptors.
 * @param plane_count  Number of planes.
 * @return Texture handle, or NULL on failure.
 */
OakTextureHandle oak_texture_create_planar(OakRendererHandle renderer,
                                           int width, int height,
                                           OakTexturePlane* planes, int plane_count);

/**
 * @brief Wrap an external GPU surface (v2).
 * @param renderer       Renderer handle.
 * @param width          Surface width.
 * @param height         Surface height.
 * @param pix_fmt        Pixel format.
 * @param external_handle Platform-specific handle (e.g. CVPixelBufferRef).
 * @param external_type   Type string describing the handle (e.g. "cvpixelbuffer", "d3d11texture").
 * @return Texture handle, or NULL on failure.
 */
OakTextureHandle oak_texture_wrap_external(OakRendererHandle renderer,
                                           int width, int height,
                                           OakRenderPixelFormat pix_fmt,
                                           void* external_handle,
                                           const char* external_type);

/**
 * @brief Destroy a texture.
 * @param renderer Renderer handle.
 * @param texture  Texture handle (NULL is silently ignored).
 */
void oak_texture_destroy(OakRendererHandle renderer, OakTextureHandle texture);

/**
 * @brief Query texture dimensions.
 * @param texture    Texture handle.
 * @param out_width  Output width.
 * @param out_height Output height.
 */
void oak_texture_size(OakTextureHandle texture, int* out_width, int* out_height);

/**
 * @brief Download texture data to CPU memory (v2).
 * @param renderer  Renderer handle.
 * @param texture   Texture handle.
 * @param width     Download width.
 * @param height    Download height.
 * @param pix_fmt   Target pixel format for download.
 * @param out_data  Pre-allocated output buffer.
 * @param stride    Line stride in bytes.
 * @return 0 on success, non-zero on failure.
 */
int oak_texture_download(OakRendererHandle renderer, OakTextureHandle texture,
                         int width, int height,
                         OakRenderPixelFormat pix_fmt,
                         void* out_data, int stride);

/**
 * @brief Query a single pixel value (v2).
 * @param renderer Renderer handle.
 * @param texture  Texture handle.
 * @param x        Normalized X coordinate [0, 1].
 * @param y        Normalized Y coordinate [0, 1].
 * @param out_rgba Output RGBA float32[4].
 * @return 0 on success, non-zero on failure.
 */
int oak_renderer_get_pixel(OakRendererHandle renderer, OakTextureHandle texture,
                           float x, float y, float* out_rgba);

/* ------------------------------------------------------------------ */
/*  Render targets                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Create an off-screen render target (FBO).
 * @param renderer  Renderer handle.
 * @param width     Target width.
 * @param height    Target height.
 * @param pix_fmt   Color buffer pixel format.
 * @param has_depth If true, attach a depth buffer.
 * @return Target handle, or NULL on failure.
 */
OakTargetHandle oak_target_create(OakRendererHandle renderer,
                                  int width, int height,
                                  OakRenderPixelFormat pix_fmt,
                                  bool has_depth);

/**
 * @brief Destroy a render target.
 * @param renderer Renderer handle.
 * @param target   Target handle (NULL is silently ignored).
 */
void            oak_target_destroy(OakRendererHandle renderer, OakTargetHandle target);

/**
 * @brief Resize a render target.
 * @param renderer Renderer handle.
 * @param target   Target handle.
 * @param width    New width.
 * @param height   New height.
 */
void            oak_target_resize(OakRendererHandle renderer, OakTargetHandle target,
                                  int width, int height);

/**
 * @brief Query render target dimensions.
 * @param target     Target handle.
 * @param out_width  Output width.
 * @param out_height Output height.
 */
void            oak_target_size(OakTargetHandle target, int* out_width, int* out_height);

/**
 * @brief Get the color texture attached to a target.
 * @param renderer Renderer handle.
 * @param target   Target handle.
 * @return Texture handle, or NULL on failure.
 */
OakTextureHandle oak_target_color_texture(OakRendererHandle renderer,
                                          OakTargetHandle target);

/**
 * @brief Detach and return the color texture from a target.
 * @param renderer Renderer handle.
 * @param target   Target handle.
 * @return Texture handle, or NULL on failure.
 * @note After detachment the target no longer owns the texture;
 *       the caller must destroy it via oak_texture_destroy.
 */
OakTextureHandle oak_target_detach_color_texture(OakRendererHandle renderer,
                                               OakTargetHandle target);

/* ------------------------------------------------------------------ */
/*  Drawing commands                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Begin rendering to a target.
 * @param renderer     Renderer handle.
 * @param target       Target handle (NULL = default framebuffer).
 * @param clear_color  RGBA float32[4] clear color. Pass NULL to skip clear.
 */
void oak_renderer_begin(OakRendererHandle renderer, OakTargetHandle target,
                        const float* clear_color);

/**
 * @brief End the current rendering pass.
 * @param renderer Renderer handle.
 */
void oak_renderer_end(OakRendererHandle renderer);

/**
 * @brief Flush all pending GPU commands.
 * @param renderer Renderer handle.
 */
void oak_renderer_flush(OakRendererHandle renderer);

/**
 * @brief Clear a texture to a solid color.
 * @param renderer     Renderer handle.
 * @param texture      Texture handle (NULL = clear current target).
 * @param clear_color  RGBA float32[4] clear color.
 */
void oak_renderer_clear_texture(OakRendererHandle renderer, OakTextureHandle texture,
                                const float* clear_color);

/**
 * @brief Draw a textured quad.
 * @param renderer   Renderer handle.
 * @param mvp_matrix 4x4 model-view-projection matrix (column-major float[16]).
 * @param texture    Texture handle (NULL = untextured).
 * @param blend_mode Blend mode.
 * @param color      RGBA tint color (float32[4]). Pass NULL for white.
 * @param uv_rect    UV rectangle {u0, v0, u1, v1}. Pass NULL for full texture.
 */
void oak_renderer_draw_quad(OakRendererHandle renderer,
                            const float* mvp_matrix,
                            OakTextureHandle texture,
                            OakBlendMode blend_mode,
                            const float* color,
                            const float* uv_rect);

/**
 * @brief Draw UTF-8 text.
 * @param renderer         Renderer handle.
 * @param utf8_text        Text string (UTF-8).
 * @param transform_matrix 4x4 transform matrix.
 * @param font_size        Font size in pixels.
 * @param color            RGBA float32[4] color.
 */
void oak_renderer_draw_text(OakRendererHandle renderer,
                            const char* utf8_text,
                            const float* transform_matrix,
                            float font_size,
                            const float* color);

/**
 * @brief Draw line segments.
 * @param renderer    Renderer handle.
 * @param points      Flat array of float32{x,y} points.
 * @param point_count Number of points.
 * @param color       RGBA float32[4] color.
 * @param line_width  Line width in pixels.
 */
void oak_renderer_draw_lines(OakRendererHandle renderer,
                             const float* points, int point_count,
                             const float* color, float line_width);

/**
 * @brief Draw a filled polygon.
 * @param renderer    Renderer handle.
 * @param points      Flat array of float32{x,y} vertices.
 * @param point_count Number of vertices.
 * @param color       RGBA float32[4] fill color.
 */
void oak_renderer_draw_polygon(OakRendererHandle renderer,
                               const float* points, int point_count,
                               const float* color);

/**
 * @brief Apply a named post-effect.
 * @param renderer      Renderer handle.
 * @param effect_name   Effect name string.
 * @param params        JSON-encoded parameter string.
 * @param source_target Source render target.
 * @param dest_target   Destination render target.
 */
void oak_renderer_apply_effect(OakRendererHandle renderer,
                               const char* effect_name,
                               const char* params,
                               OakTargetHandle source_target,
                               OakTargetHandle dest_target);

/**
 * @brief Apply a display transform (View Transform / ODT) (v2).
 * @param renderer              Renderer handle.
 * @param source_target         Source render target.
 * @param dest_target           Destination render target.
 * @param display_transform_handle Display transform handle (OakDisplayTransformHandle cast to void*).
 */
void oak_renderer_apply_display_transform(OakRendererHandle renderer,
                                          OakTargetHandle source_target,
                                          OakTargetHandle dest_target,
                                          void* display_transform_handle);

/**
 * @brief Blit YUV planes to RGBA32F ACEScg on GPU (v2).
 * @param renderer       Renderer handle.
 * @param y_tex          Y plane texture.
 * @param u_tex          U plane texture.
 * @param v_tex          V plane texture.
 * @param dest_target    Destination render target.
 * @param width          Output width.
 * @param height         Output height.
 * @param color_matrix   3x3 color matrix + offset (float[12]) for YUV->RGB conversion.
 * @param full_range     true = full range [0,255], false = limited range.
 * @param pix_fmt        Source pixel format (determines subsampling).
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

/* ------------------------------------------------------------------ */
/*  Readback                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Read back a render target to CPU memory.
 * @param renderer      Renderer handle.
 * @param target        Target handle.
 * @param out_pix_fmt   Desired output pixel format.
 * @param out_data      Output data pointer (allocated internally).
 * @param out_stride    Output line stride in bytes.
 * @return 0 on success, non-zero on failure.
 * @note Caller must free out_data via oak_renderer_free_readback.
 */
int  oak_renderer_readback(OakRendererHandle renderer, OakTargetHandle target,
                           OakRenderPixelFormat out_pix_fmt,
                           void** out_data, int* out_stride);

/**
 * @brief Free a readback buffer.
 * @param data Buffer pointer returned by oak_renderer_readback.
 */
void oak_renderer_free_readback(void* data);

/**
 * @brief Read back a frame from a source (v2).
 * @param renderer      Renderer handle.
 * @param source        Source pointer (type depends on source_type).
 * @param source_type   Source type identifier.
 * @param out_pix_fmt   Desired output pixel format.
 * @param out_frame     Output frame descriptor.
 * @return 0 on success, non-zero on failure.
 */
int oak_renderer_readback_frame(OakRendererHandle renderer,
                                void* source, int source_type,
                                OakFramePixelFormat out_pix_fmt,
                                OakFrame* out_frame);

/* ------------------------------------------------------------------ */
/*  Shader                                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Compile a shader program from vertex/fragment source.
 * @param renderer        Renderer handle.
 * @param shader_name     Debug name for the shader.
 * @param vertex_source   GLSL vertex shader source.
 * @param fragment_source GLSL fragment shader source.
 * @return Shader handle, or NULL on compilation failure.
 */
OakShaderHandle oak_shader_compile(OakRendererHandle renderer,
                                   const char* shader_name,
                                   const char* vertex_source,
                                   const char* fragment_source);

/**
 * @brief Destroy a shader.
 * @param renderer Renderer handle.
 * @param shader   Shader handle (NULL is silently ignored).
 */
void            oak_shader_destroy(OakRendererHandle renderer, OakShaderHandle shader);

/* --- Legacy JSON-based uniform passing (kept for compatibility) --- */

/**
 * @brief Draw with a custom shader using JSON uniforms.
 * @param renderer       Renderer handle.
 * @param shader         Shader handle.
 * @param uniforms_json  JSON-encoded uniform dictionary.
 * @param textures       Array of texture handles.
 * @param texture_count  Number of textures.
 * @param dest_target    Destination render target.
 */
void oak_renderer_draw_with_shader(OakRendererHandle renderer,
                                   OakShaderHandle shader,
                                   const char* uniforms_json,
                                   OakTextureHandle* textures, int texture_count,
                                   OakTargetHandle dest_target);

/**
 * @brief Draw with a custom shader to a texture using JSON uniforms.
 * @param renderer       Renderer handle.
 * @param shader         Shader handle.
 * @param uniforms_json  JSON-encoded uniform dictionary.
 * @param textures       Array of texture handles.
 * @param texture_count  Number of textures.
 * @param dest_texture   Destination texture handle.
 */
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
    int texture_unit;   /**< Valid when type == OAK_UNIFORM_TEXTURE */
    union {
        int   i;
        float f;
        float vec2[2];
        float vec3[3];
        float vec4[4];
        float mat4[16];
    } value;
} OakShaderUniform;

/**
 * @brief Draw with a custom shader using binary uniform descriptors (v2).
 * @param renderer       Renderer handle.
 * @param shader         Shader handle.
 * @param uniforms       Array of uniform descriptors.
 * @param uniform_count  Number of uniforms.
 * @param textures       Array of texture handles.
 * @param texture_count  Number of textures.
 * @param dest_target    Destination render target.
 */
void oak_renderer_draw_with_shader_ex(OakRendererHandle renderer,
                                      OakShaderHandle shader,
                                      const OakShaderUniform* uniforms,
                                      int uniform_count,
                                      OakTextureHandle* textures,
                                      int texture_count,
                                      OakTargetHandle dest_target);

/**
 * @brief Draw with a custom shader to a texture using binary uniforms (v2).
 * @param renderer       Renderer handle.
 * @param shader         Shader handle.
 * @param uniforms       Array of uniform descriptors.
 * @param uniform_count  Number of uniforms.
 * @param textures       Array of texture handles.
 * @param texture_count  Number of textures.
 * @param dest_texture   Destination texture handle.
 */
void oak_renderer_draw_with_shader_to_texture_ex(OakRendererHandle renderer,
                                                 OakShaderHandle shader,
                                                 const OakShaderUniform* uniforms,
                                                 int uniform_count,
                                                 OakTextureHandle* textures,
                                                 int texture_count,
                                                 OakTextureHandle dest_texture);

/* ------------------------------------------------------------------ */
/*  Font atlas                                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Load a font and create a glyph atlas.
 * @param renderer  Renderer handle.
 * @param font_path File path to the font (TTF/OTF).
 * @param font_size Atlas font size in pixels.
 * @return Font atlas handle, or NULL on failure.
 */
OakFontAtlasHandle oak_font_load(OakRendererHandle renderer,
                                 const char* font_path, float font_size);

/**
 * @brief Destroy a font atlas.
 * @param renderer Renderer handle.
 * @param font     Font atlas handle (NULL is silently ignored).
 */
void             oak_font_destroy(OakRendererHandle renderer, OakFontAtlasHandle font);

#ifdef __cplusplus
}
#endif

#endif /* OAK_RENDERER_API_H */
