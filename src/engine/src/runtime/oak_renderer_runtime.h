/***  Oak Video Editor - OakGL Runtime Loader  Copyright (C) 2025 mikesolar  ***/

#ifndef OAK_RENDERER_RUNTIME_H
#define OAK_RENDERER_RUNTIME_H

#include "runtime_loader.h"
#include "oak/renderer_api.h"

namespace olive {

class OakRendererRuntime : public OakRuntimeLoader {
public:
    static OakRendererRuntime* Instance();

    bool Load();

    /* --- renderer lifecycle --- */
    OakRendererHandle (*renderer_create)(const char* backend_name, void* shared_context) = nullptr;
    void (*renderer_destroy)(OakRendererHandle renderer) = nullptr;
    const char* (*renderer_backend_name)(OakRendererHandle renderer) = nullptr;
    int (*renderer_capability)(OakRendererHandle renderer, const char* capability) = nullptr;

    /* --- texture --- */
    OakTextureHandle (*texture_upload)(OakRendererHandle renderer, int width, int height,
                                       OakRenderPixelFormat pix_fmt, const void* data,
                                       size_t data_size, OakFilterMode filter,
                                       OakWrapMode wrap) = nullptr;
    OakTextureHandle (*texture_upload_from_frame)(OakRendererHandle renderer, int width,
                                                  int height, OakRenderPixelFormat pix_fmt,
                                                  const void* data, int stride) = nullptr;
    OakTextureHandle (*texture_create_planar)(OakRendererHandle renderer, int width, int height,
                                              OakTexturePlane* planes, int plane_count) = nullptr;
    OakTextureHandle (*texture_wrap_external)(OakRendererHandle renderer, int width, int height,
                                              OakRenderPixelFormat pix_fmt, void* external_handle,
                                              const char* external_type) = nullptr;
    void (*texture_destroy)(OakRendererHandle renderer, OakTextureHandle texture) = nullptr;
    void (*texture_size)(OakTextureHandle texture, int* out_width, int* out_height) = nullptr;

    /* --- target --- */
    OakTargetHandle (*target_create)(OakRendererHandle renderer, int width, int height,
                                     OakRenderPixelFormat pix_fmt, bool has_depth) = nullptr;
    void (*target_destroy)(OakRendererHandle renderer, OakTargetHandle target) = nullptr;
    void (*target_resize)(OakRendererHandle renderer, OakTargetHandle target,
                          int width, int height) = nullptr;
    void (*target_size)(OakTargetHandle target, int* out_width, int* out_height) = nullptr;
    OakTextureHandle (*target_color_texture)(OakRendererHandle renderer,
                                             OakTargetHandle target) = nullptr;
    OakTextureHandle (*target_detach_color_texture)(OakRendererHandle renderer,
                                                    OakTargetHandle target) = nullptr;

    /* --- draw --- */
    void (*renderer_begin)(OakRendererHandle renderer, OakTargetHandle target,
                           const float* clear_color) = nullptr;
    void (*renderer_end)(OakRendererHandle renderer) = nullptr;
    void (*renderer_flush)(OakRendererHandle renderer) = nullptr;
    void (*renderer_clear_texture)(OakRendererHandle renderer, OakTextureHandle texture,
                                   const float* clear_color) = nullptr;
    void (*renderer_draw_quad)(OakRendererHandle renderer, const float* mvp_matrix,
                               OakTextureHandle texture, OakBlendMode blend_mode,
                               const float* color, const float* uv_rect) = nullptr;
    void (*renderer_draw_text)(OakRendererHandle renderer, const char* utf8_text,
                               const float* transform_matrix, float font_size,
                               const float* color) = nullptr;
    void (*renderer_draw_lines)(OakRendererHandle renderer, const float* points,
                                int point_count, const float* color,
                                float line_width) = nullptr;
    void (*renderer_draw_polygon)(OakRendererHandle renderer, const float* points,
                                  int point_count, const float* color) = nullptr;
    void (*renderer_apply_effect)(OakRendererHandle renderer, const char* effect_name,
                                  const char* params, OakTargetHandle source_target,
                                  OakTargetHandle dest_target) = nullptr;
    void (*renderer_apply_display_transform)(OakRendererHandle renderer,
                                             OakTargetHandle source_target,
                                             OakTargetHandle dest_target,
                                             void* display_transform_handle) = nullptr;
    void (*renderer_blit_yuv_to_rgba)(OakRendererHandle renderer, OakTextureHandle y_tex,
                                      OakTextureHandle u_tex, OakTextureHandle v_tex,
                                      OakTargetHandle dest_target, int width, int height,
                                      const float* color_matrix, bool full_range,
                                      OakFramePixelFormat pix_fmt) = nullptr;

    /* --- readback --- */
    int (*renderer_readback)(OakRendererHandle renderer, OakTargetHandle target,
                             OakRenderPixelFormat out_pix_fmt, void** out_data,
                             int* out_stride) = nullptr;
    void (*renderer_free_readback)(void* data) = nullptr;
    int (*renderer_readback_frame)(OakRendererHandle renderer, void* source,
                                   int source_type, OakFramePixelFormat out_pix_fmt,
                                   OakFrame* out_frame) = nullptr;
    int (*texture_download)(OakRendererHandle renderer, OakTextureHandle texture,
                            int width, int height,
                            OakRenderPixelFormat pix_fmt,
                            void* out_data, int stride) = nullptr;
    int (*renderer_get_pixel)(OakRendererHandle renderer, OakTextureHandle texture,
                              float x, float y, float* out_rgba) = nullptr;

    /* --- shader --- */
    OakShaderHandle (*shader_compile)(OakRendererHandle renderer, const char* shader_name,
                                      const char* vertex_source,
                                      const char* fragment_source) = nullptr;
    void (*shader_destroy)(OakRendererHandle renderer, OakShaderHandle shader) = nullptr;
    void (*renderer_draw_with_shader)(OakRendererHandle renderer, OakShaderHandle shader,
                                      const char* uniforms_json,
                                      OakTextureHandle* textures, int texture_count,
                                      OakTargetHandle dest_target) = nullptr;

    /* --- v2: binary uniform --- */
    void (*renderer_draw_with_shader_ex)(OakRendererHandle renderer, OakShaderHandle shader,
                                         const OakShaderUniform* uniforms, int uniform_count,
                                         OakTextureHandle* textures, int texture_count,
                                         OakTargetHandle dest_target) = nullptr;
    void (*renderer_draw_with_shader_to_texture_ex)(OakRendererHandle renderer,
                                                    OakShaderHandle shader,
                                                    const OakShaderUniform* uniforms,
                                                    int uniform_count,
                                                    OakTextureHandle* textures,
                                                    int texture_count,
                                                    OakTextureHandle dest_texture) = nullptr;

    /* --- font --- */
    OakFontAtlasHandle (*font_load)(OakRendererHandle renderer, const char* font_path,
                                    float font_size) = nullptr;
    void (*font_destroy)(OakRendererHandle renderer, OakFontAtlasHandle font) = nullptr;

private:
    OakRendererRuntime() = default;
};

} // namespace olive

#endif // OAK_RENDERER_RUNTIME_H
