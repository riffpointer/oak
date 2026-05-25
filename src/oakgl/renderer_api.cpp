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

#if defined(__APPLE__)
#include <dlfcn.h>
#elif defined(__linux__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

/* ================================================================ */
/*  Runtime oakcolor loader (for display transform)                    */
/* ================================================================ */

struct OakColorFunctions {
    void* handle = nullptr;

    typedef int (*display_transform_apply_fn)(void* transform,
                                              int width, int height,
                                              const float* in_data, float* out_data,
                                              int pix_layout);
    display_transform_apply_fn display_transform_apply = nullptr;

    bool Load() {
        if (handle) return true;
#if defined(__APPLE__)
        handle = dlopen("liboakcolor.dylib", RTLD_NOW | RTLD_LOCAL);
#elif defined(__linux__)
        handle = dlopen("liboakcolor.so", RTLD_NOW | RTLD_LOCAL);
#elif defined(_WIN32)
        handle = (void*)LoadLibraryA("oakcolor.dll");
#endif
        if (!handle) return false;

        display_transform_apply = reinterpret_cast<display_transform_apply_fn>(
            dlsym(handle, "oak_display_transform_apply"));
        return display_transform_apply != nullptr;
    }

    ~OakColorFunctions() {
        if (handle) {
#if defined(__APPLE__) || defined(__linux__)
            dlclose(handle);
#elif defined(_WIN32)
            FreeLibrary((HMODULE)handle);
#endif
        }
    }
};

static OakColorFunctions* GetOakColor() {
    static OakColorFunctions g_oakcolor;
    if (!g_oakcolor.handle) {
        g_oakcolor.Load();
    }
    return &g_oakcolor;
}

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

static int GetChannelsFromPixelFormat(OakRenderPixelFormat fmt)
{
    switch (fmt) {
    case OAK_RENDER_PIX_FMT_R8:
    case OAK_RENDER_PIX_FMT_R16:
    case OAK_RENDER_PIX_FMT_R32F:
    case OAK_RENDER_PIX_FMT_R8_SNORM:
        return 1;
    case OAK_RENDER_PIX_FMT_RG8:
    case OAK_RENDER_PIX_FMT_RG8_SNORM:
        return 2;
    case OAK_RENDER_PIX_FMT_RGBA8:
    case OAK_RENDER_PIX_FMT_RGBA16:
    case OAK_RENDER_PIX_FMT_RGBA32F:
        return 4;
    }
    return 4;
}

int oak_texture_download(OakRendererHandle renderer, OakTextureHandle texture,
                         int width, int height,
                         OakRenderPixelFormat pix_fmt,
                         void* out_data, int stride)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !texture || !out_data) return -1;
    GLuint tex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(texture));
    int channels = GetChannelsFromPixelFormat(pix_fmt);
    r->DownloadTexture(tex, width, height, 1, channels, pix_fmt, out_data, stride);
    return 0;
}

int oak_renderer_get_pixel(OakRendererHandle renderer, OakTextureHandle texture,
                           float x, float y, float* out_rgba)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !texture || !out_rgba) return -1;
    GLuint tex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(texture));
    r->GetPixel(tex, x, y, out_rgba);
    return 0;
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
    // Target size requires renderer context. For now, caller should track size.
    if (out_width)  *out_width  = 0;
    if (out_height) *out_height = 0;
}

OakTextureHandle oak_target_color_texture(OakRendererHandle renderer,
                                          OakTargetHandle target)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !target) return nullptr;
    GLuint fbo = static_cast<GLuint>(reinterpret_cast<uintptr_t>(target));
    GLuint tex = r->GetTargetColorTexture(fbo);
    if (tex == 0) return nullptr;
    return reinterpret_cast<OakTextureHandle>(static_cast<uintptr_t>(tex));
}

OakTextureHandle oak_target_detach_color_texture(OakRendererHandle renderer,
                                               OakTargetHandle target)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !target) return nullptr;
    GLuint fbo = static_cast<GLuint>(reinterpret_cast<uintptr_t>(target));
    GLuint tex = r->DetachTargetColorTexture(fbo);
    if (tex == 0) return nullptr;
    return reinterpret_cast<OakTextureHandle>(static_cast<uintptr_t>(tex));
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

void oak_renderer_flush(OakRendererHandle renderer)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;
    r->Flush();
}

void oak_renderer_clear_texture(OakRendererHandle renderer, OakTextureHandle texture,
                                const float* clear_color)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;
    GLuint tex = texture ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(texture)) : 0;
    r->ClearTexture(tex, clear_color);
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

void oak_renderer_apply_display_transform(OakRendererHandle renderer,
                                          OakTargetHandle source_target,
                                          OakTargetHandle dest_target,
                                          void* display_transform_handle)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !source_target || !display_transform_handle) return;

    OakColorFunctions* color = GetOakColor();
    if (!color || !color->display_transform_apply) return;

    GLuint src_fbo = static_cast<GLuint>(reinterpret_cast<uintptr_t>(source_target));
    int width = 0, height = 0;
    if (!r->GetTargetSize(src_fbo, &width, &height)) return;

    // Readback source to CPU buffer (RGBA32F)
    void* cpu_data = nullptr;
    int stride = 0;
    if (!r->Readback(src_fbo, width, height, OAK_RENDER_PIX_FMT_RGBA32F,
                     &cpu_data, &stride)) {
        return;
    }

    // Apply display transform in-place
    color->display_transform_apply(display_transform_handle,
                                   width, height,
                                   static_cast<const float*>(cpu_data),
                                   static_cast<float*>(cpu_data),
                                   0);

    // Upload back to destination
    GLuint dst_tex = 0;
    if (dest_target) {
        GLuint dst_fbo = static_cast<GLuint>(reinterpret_cast<uintptr_t>(dest_target));
        dst_tex = r->GetTargetColorTexture(dst_fbo);
        if (!dst_tex) {
            // Destination target exists but has no color texture? Fallback to source
            dst_tex = r->GetTargetColorTexture(src_fbo);
            dst_fbo = src_fbo;
        }
    }

    if (dst_tex) {
        r->UploadTexture(dst_tex, width, height, 1, 4,
                         OAK_RENDER_PIX_FMT_RGBA32F,
                         cpu_data, stride);
    }

    std::free(cpu_data);
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

void oak_renderer_draw_with_shader_to_texture(OakRendererHandle renderer,
                                              OakShaderHandle shader,
                                              const char *uniforms_json,
                                              OakTextureHandle *textures, int texture_count,
                                              OakTextureHandle dest_texture)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;

    GLuint program = shader ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(shader)) : 0;
    GLuint dst = dest_texture ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(dest_texture)) : 0;

    GLuint *tex_ids = nullptr;
    if (texture_count > 0 && textures) {
        tex_ids = new GLuint[texture_count];
        for (int i = 0; i < texture_count; i++) {
            tex_ids[i] = static_cast<GLuint>(reinterpret_cast<uintptr_t>(textures[i]));
        }
    }

    r->DrawWithShaderToTexture(program, uniforms_json, tex_ids, texture_count, dst);

    delete[] tex_ids;
}

/* ------------------------------------------------------------------ */
/*  v2: Binary uniform descriptor (zero string overhead)                */
/* ------------------------------------------------------------------ */

void oak_renderer_draw_with_shader_ex(OakRendererHandle renderer,
                                      OakShaderHandle shader,
                                      const OakShaderUniform* uniforms,
                                      int uniform_count,
                                      OakTextureHandle* textures,
                                      int texture_count,
                                      OakTargetHandle dest_target)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;

    GLuint program = shader ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(shader)) : 0;
    GLuint dst = dest_target ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(dest_target)) : 0;

    GLuint *tex_ids = nullptr;
    if (texture_count > 0 && textures) {
        tex_ids = new GLuint[texture_count];
        for (int i = 0; i < texture_count; i++) {
            tex_ids[i] = static_cast<GLuint>(reinterpret_cast<uintptr_t>(textures[i]));
        }
    }

    r->DrawWithShaderEx(program, uniforms, uniform_count, tex_ids, texture_count, dst);

    delete[] tex_ids;
}

void oak_renderer_draw_with_shader_to_texture_ex(OakRendererHandle renderer,
                                                 OakShaderHandle shader,
                                                 const OakShaderUniform* uniforms,
                                                 int uniform_count,
                                                 OakTextureHandle* textures,
                                                 int texture_count,
                                                 OakTextureHandle dest_texture)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;

    GLuint program = shader ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(shader)) : 0;
    GLuint dst = dest_texture ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(dest_texture)) : 0;

    GLuint *tex_ids = nullptr;
    if (texture_count > 0 && textures) {
        tex_ids = new GLuint[texture_count];
        for (int i = 0; i < texture_count; i++) {
            tex_ids[i] = static_cast<GLuint>(reinterpret_cast<uintptr_t>(textures[i]));
        }
    }

    r->DrawWithShaderToTextureEx(program, uniforms, uniform_count, tex_ids, texture_count, dst);

    delete[] tex_ids;
}

/* ------------------------------------------------------------------ */
/*  v2: Planar texture upload                                           */
/* ------------------------------------------------------------------ */

OakTextureHandle oak_texture_create_planar(OakRendererHandle renderer,
                                           int width, int height,
                                           OakTexturePlane* planes, int plane_count)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !planes || plane_count <= 0) return nullptr;

    // Create first texture as base handle; subsequent planes are +offset
    GLuint base_tex = r->CreateTexture(planes[0].width, planes[0].height, 1,
                                       (planes[0].pix_fmt == OAK_RENDER_PIX_FMT_RG8) ? 2 : 1,
                                       planes[0].pix_fmt,
                                       planes[0].data, planes[0].stride);
    if (base_tex == 0) return nullptr;

    // Upload remaining planes (if any) - their handles will be base_tex + i
    for (int i = 1; i < plane_count; i++) {
        GLuint tex = r->CreateTexture(planes[i].width, planes[i].height, 1,
                                      (planes[i].pix_fmt == OAK_RENDER_PIX_FMT_RG8) ? 2 : 1,
                                      planes[i].pix_fmt,
                                      planes[i].data, planes[i].stride);
        if (tex == 0) {
            // Best-effort cleanup: destroy already created textures
            for (int j = 0; j < i; j++) {
                r->DestroyTexture(base_tex + j);
            }
            return nullptr;
        }
        // Expect contiguous allocation
        if (tex != base_tex + i) {
            // Non-contiguous, destroy and return error
            for (int j = 0; j <= i; j++) {
                r->DestroyTexture(base_tex + j);
            }
            return nullptr;
        }
    }

    return reinterpret_cast<OakTextureHandle>(static_cast<uintptr_t>(base_tex));
}

/* ------------------------------------------------------------------ */
/*  v2: External surface wrap                                           */
/* ------------------------------------------------------------------ */

OakTextureHandle oak_texture_wrap_external(OakRendererHandle renderer,
                                           int width, int height,
                                           OakRenderPixelFormat pix_fmt,
                                           void* external_handle,
                                           const char* external_type)
{
    (void)width;
    (void)height;
    (void)pix_fmt;
    (void)external_handle;
    (void)external_type;
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return nullptr;

    // TODO: implement platform-specific external texture wrapping
    // macOS: CVOpenGLTextureCacheCreateTextureFromImage
    // Windows: ID3D11Texture2D shared handle
    // Linux: EGLImageKHR
    return nullptr;
}

/* ------------------------------------------------------------------ */
/*  v2: GPU YUV->RGBA32F ACEScg blit                                    */
/* ------------------------------------------------------------------ */

void oak_renderer_blit_yuv_to_rgba(OakRendererHandle renderer,
                                   OakTextureHandle y_tex,
                                   OakTextureHandle u_tex,
                                   OakTextureHandle v_tex,
                                   OakTargetHandle dest_target,
                                   int width, int height,
                                   const float* color_matrix,
                                   bool full_range,
                                   OakFramePixelFormat pix_fmt)
{
    (void)pix_fmt;
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r) return;

    GLuint y = y_tex ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(y_tex)) : 0;
    GLuint u = u_tex ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(u_tex)) : 0;
    GLuint v = v_tex ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(v_tex)) : 0;
    GLuint dst = dest_target ? static_cast<GLuint>(reinterpret_cast<uintptr_t>(dest_target)) : 0;

    r->BlitYUVToRGBA(y, u, v, dst, width, height, color_matrix, full_range);
}

/* ------------------------------------------------------------------ */
/*  v2: Frame readback                                                  */
/* ------------------------------------------------------------------ */

int oak_renderer_readback_frame(OakRendererHandle renderer,
                                void* source, int source_type,
                                OakFramePixelFormat out_pix_fmt,
                                OakFrame* out_frame)
{
    auto *r = reinterpret_cast<oakgl::OpenGLRenderer*>(renderer);
    if (!r || !out_frame) return -1;

    // Determine source FBO / texture
    GLuint src_fbo = 0;
    int width = 0, height = 0;

    if (source_type == 0) { // texture
        OakTextureHandle tex = reinterpret_cast<OakTextureHandle>(source);
        GLuint gl_tex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(tex));
        (void)gl_tex;
        // TODO: render texture to temp FBO then readback
        return -1;
    } else if (source_type == 1) { // target
        OakTargetHandle tgt = reinterpret_cast<OakTargetHandle>(source);
        src_fbo = static_cast<GLuint>(reinterpret_cast<uintptr_t>(tgt));
    } else {
        src_fbo = 0; // current target
    }

    (void)src_fbo;

    // Map out_pix_fmt to OakRenderPixelFormat
    OakRenderPixelFormat render_fmt = OAK_RENDER_PIX_FMT_RGBA32F;
    switch (out_pix_fmt) {
    case OAK_FRAME_PIX_RGBA8:  render_fmt = OAK_RENDER_PIX_FMT_RGBA8; break;
    case OAK_FRAME_PIX_RGBA16: render_fmt = OAK_RENDER_PIX_FMT_RGBA16; break;
    case OAK_FRAME_PIX_RGBA32F: render_fmt = OAK_RENDER_PIX_FMT_RGBA32F; break;
    default: break;
    }

    (void)render_fmt;

    // TODO: implement readback via r->Readback()
    // For now, stub
    out_frame->width = width;
    out_frame->height = height;
    out_frame->pix_fmt = out_pix_fmt;
    out_frame->storage = OAK_FRAME_CPU;
    out_frame->colorspace = "ACES - ACEScg";
    out_frame->planes = 1;
    out_frame->data[0] = nullptr;
    out_frame->stride[0] = 0;
    out_frame->internal = nullptr;
    return -1;
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
