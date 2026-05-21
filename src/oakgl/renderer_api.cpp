#include "oak/renderer_api.h"
#include "oakgl_internal.h"

#include <cstdlib>
#include <cstring>

/* ------------------------------------------------------------------ */
/*  Renderer lifecycle                                                  */
/* ------------------------------------------------------------------ */

OakRendererHandle oak_renderer_create(const char* backend_name, void* shared_context) {
    (void)shared_context;
    if (!backend_name) return nullptr;
    // TODO: dispatch to OpenGL / Vulkan / Metal / CPU backend
    auto* r = new OakRenderer();
    r->backend_name = backend_name;
    return r;
}

void oak_renderer_destroy(OakRendererHandle renderer) {
    if (!renderer) return;
    // TODO: release all owned textures, targets, shaders
    delete renderer;
}

const char* oak_renderer_backend_name(OakRendererHandle renderer) {
    return renderer ? renderer->backend_name.c_str() : nullptr;
}

int oak_renderer_capability(OakRendererHandle renderer, const char* capability) {
    (void)renderer; (void)capability;
    return 0; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Texture management                                                  */
/* ------------------------------------------------------------------ */

OakTextureHandle oak_texture_upload(OakRendererHandle renderer,
                                    int width, int height,
                                    OakRenderPixelFormat pix_fmt,
                                    const void* data, size_t data_size,
                                    OakFilterMode filter,
                                    OakWrapMode wrap) {
    (void)renderer; (void)width; (void)height; (void)pix_fmt;
    (void)data; (void)data_size; (void)filter; (void)wrap;
    return nullptr; /* TODO */
}

OakTextureHandle oak_texture_upload_from_frame(OakRendererHandle renderer,
                                               int width, int height,
                                               OakRenderPixelFormat pix_fmt,
                                               const void* data, int stride) {
    (void)renderer; (void)width; (void)height; (void)pix_fmt;
    (void)data; (void)stride;
    return nullptr; /* TODO */
}

void oak_texture_destroy(OakRendererHandle renderer, OakTextureHandle texture) {
    (void)renderer;
    delete texture;
}

void oak_texture_size(OakTextureHandle texture, int* out_width, int* out_height) {
    if (!texture) return;
    if (out_width)  *out_width  = texture->width;
    if (out_height) *out_height = texture->height;
}

/* ------------------------------------------------------------------ */
/*  Render target                                                       */
/* ------------------------------------------------------------------ */

OakTargetHandle oak_target_create(OakRendererHandle renderer,
                                  int width, int height,
                                  OakRenderPixelFormat pix_fmt,
                                  bool has_depth) {
    (void)renderer;
    auto* t = new OakTarget();
    t->width = width;
    t->height = height;
    t->pix_fmt = pix_fmt;
    t->has_depth = has_depth;
    // TODO: create native FBO / render pass
    return t;
}

void oak_target_destroy(OakRendererHandle renderer, OakTargetHandle target) {
    (void)renderer;
    delete target;
}

void oak_target_resize(OakRendererHandle renderer, OakTargetHandle target,
                       int width, int height) {
    (void)renderer;
    if (!target) return;
    target->width = width;
    target->height = height;
    // TODO: recreate native resources
}

void oak_target_size(OakTargetHandle target, int* out_width, int* out_height) {
    if (!target) return;
    if (out_width)  *out_width  = target->width;
    if (out_height) *out_height = target->height;
}

/* ------------------------------------------------------------------ */
/*  High-level draw commands                                            */
/* ------------------------------------------------------------------ */

void oak_renderer_begin(OakRendererHandle renderer, OakTargetHandle target,
                        const float* clear_color) {
    (void)renderer; (void)target; (void)clear_color;
    // TODO: bind target, clear, begin command recording
}

void oak_renderer_end(OakRendererHandle renderer) {
    (void)renderer;
    // TODO: submit command buffer / swap
}

void oak_renderer_draw_quad(OakRendererHandle renderer,
                            const float* mvp_matrix,
                            OakTextureHandle texture,
                            OakBlendMode blend_mode,
                            const float* color,
                            const float* uv_rect) {
    (void)renderer; (void)mvp_matrix; (void)texture;
    (void)blend_mode; (void)color; (void)uv_rect;
    // TODO: emit draw call
}

void oak_renderer_draw_text(OakRendererHandle renderer,
                            const char* utf8_text,
                            const float* transform_matrix,
                            float font_size,
                            const float* color) {
    (void)renderer; (void)utf8_text; (void)transform_matrix;
    (void)font_size; (void)color;
    // TODO: generate glyph quads, emit draw call
}

void oak_renderer_draw_lines(OakRendererHandle renderer,
                             const float* points, int point_count,
                             const float* color, float line_width) {
    (void)renderer; (void)points; (void)point_count;
    (void)color; (void)line_width;
    // TODO: emit line draw call
}

void oak_renderer_draw_polygon(OakRendererHandle renderer,
                               const float* points, int point_count,
                               const float* color) {
    (void)renderer; (void)points; (void)point_count; (void)color;
    // TODO: emit polygon fill draw call
}

void oak_renderer_apply_effect(OakRendererHandle renderer,
                               const char* effect_name,
                               const char* params,
                               OakTargetHandle source_target,
                               OakTargetHandle dest_target) {
    (void)renderer; (void)effect_name; (void)params;
    (void)source_target; (void)dest_target;
    // TODO: dispatch to built-in post-processing shaders
}

/* ------------------------------------------------------------------ */
/*  Readback                                                            */
/* ------------------------------------------------------------------ */

int oak_renderer_readback(OakRendererHandle renderer, OakTargetHandle target,
                          OakRenderPixelFormat out_pix_fmt,
                          void** out_data, int* out_stride) {
    (void)renderer; (void)target; (void)out_pix_fmt;
    if (out_data)   *out_data   = nullptr;
    if (out_stride) *out_stride = 0;
    return -1; /* TODO */
}

void oak_renderer_free_readback(void* data) {
    std::free(data);
}

/* ------------------------------------------------------------------ */
/*  Shader interface                                                    */
/* ------------------------------------------------------------------ */

OakShaderHandle oak_shader_compile(OakRendererHandle renderer,
                                   const char* shader_name,
                                   const char* vertex_source,
                                   const char* fragment_source) {
    (void)renderer; (void)shader_name;
    (void)vertex_source; (void)fragment_source;
    return nullptr; /* TODO */
}

void oak_shader_destroy(OakRendererHandle renderer, OakShaderHandle shader) {
    (void)renderer;
    delete shader;
}

void oak_renderer_draw_with_shader(OakRendererHandle renderer,
                                   OakShaderHandle shader,
                                   const char* uniforms_json,
                                   OakTextureHandle* textures, int texture_count,
                                   OakTargetHandle dest_target) {
    (void)renderer; (void)shader; (void)uniforms_json;
    (void)textures; (void)texture_count; (void)dest_target;
    // TODO: parse uniforms_json, bind textures, emit custom shader draw call
}

/* ------------------------------------------------------------------ */
/*  Font atlas                                                          */
/* ------------------------------------------------------------------ */

OakFontAtlasHandle oak_font_load(OakRendererHandle renderer,
                                 const char* font_path, float font_size) {
    (void)renderer;
    auto* f = new OakFontAtlas();
    f->font_path = font_path ? font_path : "";
    f->font_size = font_size;
    // TODO: rasterize glyphs, create texture
    return f;
}

void oak_font_destroy(OakRendererHandle renderer, OakFontAtlasHandle font) {
    (void)renderer;
    delete font;
}
