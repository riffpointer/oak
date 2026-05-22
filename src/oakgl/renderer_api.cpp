/***

  oakgl.so C API Implementation
  Copyright (C) 2025 mikesolar

  This file implements the pure C ABI declared in oak/renderer_api.h
  by forwarding to the internal OpenGLRenderer C++ class.

***/

#include "oak/renderer_api.h"
#include "openglrenderer.h"

#include <cstdlib>
#include <cstring>

/* ------------------------------------------------------------------ */
/*  Renderer lifecycle                                                  */
/* ------------------------------------------------------------------ */

OakRendererHandle oak_renderer_create(const char *backend_name, void *shared_context)
{
    (void)backend_name; // Only "opengl" is supported for now
    (void)shared_context;

    auto *r = new oakgl::OpenGLRenderer();
    if (!r->Init()) {
        delete r;
        return nullptr;
    }
    return reinterpret_cast<OakRendererHandle>(r);
}

void oak_renderer_destroy(OakRendererHandle renderer)
{
    if (!renderer) return;
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    r->Destroy();
    delete r;
}

const char *oak_renderer_backend_name(OakRendererHandle renderer)
{
    (void)renderer;
    return "opengl";
}

int oak_renderer_capability(OakRendererHandle renderer, const char *capability)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !capability) return 0;

    if (std::strcmp(capability, "max_texture_size") == 0) {
        GLint max_size = 0;
        if (r->context()) {
            r->context()->functions()->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
        }
        return max_size;
    }
    if (std::strcmp(capability, "supports_float_texture") == 0) {
        return 1;
    }
    if (std::strcmp(capability, "supports_compute") == 0) {
        return 0; // OpenGL compute not exposed yet
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Texture management                                                  */
/* ------------------------------------------------------------------ */

OakTextureHandle oak_texture_upload(OakRendererHandle renderer,
                                    int width, int height,
                                    OakRenderPixelFormat pix_fmt,
                                    const void *data, size_t data_size,
                                    OakFilterMode filter,
                                    OakWrapMode wrap)
{
    (void)data_size;
    (void)filter;
    (void)wrap;
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return nullptr;

    int channels = 4;
    switch (pix_fmt) {
    case OAK_RENDER_PIX_FMT_R8:  channels = 1; break;
    case OAK_RENDER_PIX_FMT_RG8: channels = 2; break;
    default: channels = 4; break;
    }

    GLuint tex = r->CreateTexture(width, height, 1, channels, pix_fmt, data, 0);
    if (tex == 0) return nullptr;

    OakTextureHandle handle = reinterpret_cast<OakTextureHandle>(
        static_cast<uintptr_t>(tex));
    return handle;
}

OakTextureHandle oak_texture_upload_from_frame(OakRendererHandle renderer,
                                               int width, int height,
                                               OakRenderPixelFormat pix_fmt,
                                               const void *data, int stride)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return nullptr;

    int channels = 4;
    switch (pix_fmt) {
    case OAK_RENDER_PIX_FMT_R8:  channels = 1; break;
    case OAK_RENDER_PIX_FMT_RG8: channels = 2; break;
    default: channels = 4; break;
    }

    GLuint tex = r->CreateTexture(width, height, 1, channels, pix_fmt, data, stride);
    if (tex == 0) return nullptr;

    return reinterpret_cast<OakTextureHandle>(static_cast<uintptr_t>(tex));
}

void oak_texture_destroy(OakRendererHandle renderer, OakTextureHandle texture)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !texture) return;
    GLuint tex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(texture));
    r->DestroyTexture(tex);
}

void oak_texture_size(OakTextureHandle texture, int *out_width, int *out_height)
{
    (void)texture;
    // TODO: store metadata with handle
    if (out_width)  *out_width  = 0;
    if (out_height) *out_height = 0;
}

/* ------------------------------------------------------------------ */
/*  Render target                                                       */
/* ------------------------------------------------------------------ */

OakTargetHandle oak_target_create(OakRendererHandle renderer,
                                  int width, int height,
                                  OakRenderPixelFormat pix_fmt,
                                  bool has_depth)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return nullptr;

    GLuint fbo = r->CreateTarget(width, height, pix_fmt, has_depth);
    if (fbo == 0) return nullptr;

    return reinterpret_cast<OakTargetHandle>(static_cast<uintptr_t>(fbo));
}

void oak_target_destroy(OakRendererHandle renderer, OakTargetHandle target)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !target) return;
    GLuint fbo = static_cast<GLuint>(reinterpret_cast<uintptr_t>(target));
    r->DestroyTarget(fbo);
}

void oak_target_resize(OakRendererHandle renderer, OakTargetHandle target,
                       int width, int height)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !target) return;
    GLuint fbo = static_cast<GLuint>(reinterpret_cast<uintptr_t>(target));
    r->ResizeTarget(fbo, width, height);
}

void oak_target_size(OakTargetHandle target, int *out_width, int *out_height)
{
    (void)target;
    // TODO: store metadata with handle
    if (out_width)  *out_width  = 0;
    if (out_height) *out_height = 0;
}

/* ------------------------------------------------------------------ */
/*  Drawing commands                                                    */
/* ------------------------------------------------------------------ */

void oak_renderer_begin(OakRendererHandle renderer, OakTargetHandle target,
                        const float *clear_color)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;
    GLuint fbo = target ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(target)) : 0;
    r->BeginFrame(fbo, clear_color);
}

void oak_renderer_end(OakRendererHandle renderer)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;
    r->EndFrame();
}

void oak_renderer_draw_quad(OakRendererHandle renderer,
                            const float *mvp_matrix,
                            OakTextureHandle texture,
                            OakBlendMode blend_mode,
                            const float *color,
                            const float *uv_rect)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;

    GLuint tex = texture ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(texture)) : 0;
    r->DrawQuad(mvp_matrix, tex, blend_mode, color, uv_rect);
}

void oak_renderer_draw_text(OakRendererHandle renderer,
                            const char *utf8_text,
                            const float *transform_matrix,
                            float font_size,
                            const float *color)
{
    (void)renderer;
    (void)utf8_text;
    (void)transform_matrix;
    (void)font_size;
    (void)color;
    // TODO: implement text rendering using font atlas
}

void oak_renderer_draw_lines(OakRendererHandle renderer,
                             const float *points, int point_count,
                             const float *color, float line_width)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;
    r->DrawLines(points, point_count, color, line_width);
}

void oak_renderer_draw_polygon(OakRendererHandle renderer,
                               const float *points, int point_count,
                               const float *color)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;
    r->DrawPolygon(points, point_count, color);
}

void oak_renderer_apply_effect(OakRendererHandle renderer,
                               const char *effect_name,
                               const char *params,
                               OakTargetHandle source_target,
                               OakTargetHandle dest_target)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;
    GLuint src = source_target ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(source_target)) : 0;
    GLuint dst = dest_target ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(dest_target)) : 0;
    r->ApplyEffect(effect_name, params, src, dst);
}

/* ------------------------------------------------------------------ */
/*  Readback                                                            */
/* ------------------------------------------------------------------ */

int oak_renderer_readback(OakRendererHandle renderer, OakTargetHandle target,
                          OakRenderPixelFormat out_pix_fmt,
                          void **out_data, int *out_stride)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return -1;

    GLuint fbo = target ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(target)) : 0;

    // Need target dimensions - for now assume caller knows, or we can query
    // TODO: store target metadata to get width/height from handle
    // Using 0x0 will fail, so we return error until metadata is stored
    (void)out_pix_fmt;
    if (out_data) *out_data = nullptr;
    if (out_stride) *out_stride = 0;
    return -1; /* TODO: need width/height from target handle metadata */
}

void oak_renderer_free_readback(void *data)
{
    std::free(data);
}

/* ------------------------------------------------------------------ */
/*  Shader interface                                                    */
/* ------------------------------------------------------------------ */

OakShaderHandle oak_shader_compile(OakRendererHandle renderer,
                                   const char *shader_name,
                                   const char *vertex_source,
                                   const char *fragment_source)
{
    (void)shader_name;
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return nullptr;

    GLuint program = r->CompileShader(vertex_source, fragment_source);
    if (program == 0) return nullptr;

    return reinterpret_cast<OakShaderHandle>(static_cast<uintptr_t>(program));
}

void oak_shader_destroy(OakRendererHandle renderer, OakShaderHandle shader)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !shader) return;
    GLuint program = static_cast<GLuint>(reinterpret_cast<uintptr_t>(shader));
    r->DestroyShader(program);
}

void oak_renderer_draw_with_shader(OakRendererHandle renderer,
                                   OakShaderHandle shader,
                                   const char *uniforms_json,
                                   OakTextureHandle *textures, int texture_count,
                                   OakTargetHandle dest_target)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;

    GLuint program = shader ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(shader)) : 0;
    GLuint dst = dest_target ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(dest_target)) : 0;

    // Convert texture handles to GLuint array
    GLuint *tex_ids = nullptr;
    if (texture_count > 0 && textures) {
        tex_ids = new GLuint[texture_count];
        for (int i = 0; i < texture_count; i++) {
            tex_ids[i] = static_cast<GLuint>(reinterpret_cast<uintptr_t>(textures[i]));
        }
    }

    r->DrawWithShader(program, uniforms_json, tex_ids, texture_count, dst);

    delete[] tex_ids;
}

/* ------------------------------------------------------------------ */
/*  Font atlas                                                          */
/* ------------------------------------------------------------------ */

OakFontAtlasHandle oak_font_load(OakRendererHandle renderer,
                                 const char *font_path, float font_size)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return nullptr;

    GLuint font = r->LoadFont(font_path, font_size);
    if (font == 0) return nullptr;

    return reinterpret_cast<OakFontAtlasHandle>(static_cast<uintptr_t>(font));
}

void oak_font_destroy(OakRendererHandle renderer, OakFontAtlasHandle font)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !font) return;
    GLuint f = static_cast<GLuint>(reinterpret_cast<uintptr_t>(font));
    r->DestroyFont(f);
}
