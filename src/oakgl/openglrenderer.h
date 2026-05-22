/***

  Oak Video Editor - OpenGL Render Backend
  Copyright (C) 2025 mikesolar

  This module is the ONLY module in the project that knows about OpenGL.
  All external code must go through oak_renderer_api.h (pure C ABI).

***/

#ifndef OAKGL_OPENGLRENDERER_H
#define OAKGL_OPENGLRENDERER_H

#include <QMutex>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOffscreenSurface>
#include <QObject>
#include <QVariant>

#include "oak/renderer_api.h"

namespace oakgl
{

/**
 * @brief Self-contained OpenGL renderer backend.
 *
 * No dependency on engine/node/job/render systems.
 * All external communication goes through the C API in renderer_api.cpp.
 */
class OpenGLRenderer : public QObject {
    Q_OBJECT
public:
    OpenGLRenderer(QObject *parent = nullptr);
    virtual ~OpenGLRenderer();

    bool Init();
    void Destroy();

    /* ---- Texture ---- */
    GLuint CreateTexture(int width, int height, int depth, int channels,
                         OakRenderPixelFormat fmt,
                         const void *data = nullptr, int linesize = 0);
    void DestroyTexture(GLuint tex);
    void UploadTexture(GLuint tex, int width, int height, int depth, int channels,
                       OakRenderPixelFormat fmt, const void *data, int linesize);
    void DownloadTexture(GLuint tex, int width, int height, int depth, int channels,
                         OakRenderPixelFormat fmt, void *data, int linesize);

    /* ---- Render Target (FBO) ---- */
    GLuint CreateTarget(int width, int height, OakRenderPixelFormat fmt,
                        bool has_depth);
    void DestroyTarget(GLuint fbo);
    void ResizeTarget(GLuint fbo, int width, int height);

    /* ---- Shader ---- */
    GLuint CompileShader(const char *vert_src, const char *frag_src);
    void DestroyShader(GLuint program);

    /* ---- Drawing ---- */
    void BeginFrame(GLuint fbo, const float *clear_color);
    void EndFrame();

    void DrawQuad(const float *mvp, GLuint tex, OakBlendMode blend,
                  const float *color, const float *uv_rect);

    void DrawLines(const float *points, int point_count,
                   const float *color, float line_width);

    void DrawPolygon(const float *points, int point_count,
                     const float *color);

    void ApplyEffect(const char *effect_name, const char *params,
                     GLuint src_fbo, GLuint dst_fbo);

    void DrawWithShader(GLuint program, const char *uniforms_json,
                        GLuint *textures, int tex_count, GLuint dst_fbo);

    /* ---- Readback ---- */
    bool Readback(GLuint fbo, int width, int height, OakRenderPixelFormat fmt,
                  void **out_data, int *out_stride);

    /* ---- Font ---- */
    GLuint LoadFont(const char *path, float size);
    void DestroyFont(GLuint font);

    /* ---- Pixel query ---- */
    void GetPixel(GLuint tex, float x, float y, float *out_rgba);

    /* ---- Flush ---- */
    void Flush();

    /* ---- Context access ---- */
    QOpenGLContext *context() const { return context_; }
    bool EnsureContextCurrent(const char *caller);

private:
    static GLint GetInternalFormat(OakRenderPixelFormat fmt, int channels);
    static GLenum GetPixelType(OakRenderPixelFormat fmt);
    static GLenum GetPixelFormat(int channels);

    void PrepareInputTexture(GLenum target, OakFilterMode filter);
    void ClearDestinationInternal(float r, float g, float b, float a);

    GLuint CompileShaderInternal(GLenum type, const QString &code);

    void AttachTextureAsDestination(GLuint texture);
    void DetachTextureAsDestination();

    QOpenGLContext *context_;
    QOpenGLFunctions *functions_;
    QOffscreenSurface surface_;
    GLuint framebuffer_;

    struct TargetInfo {
        int width = 0;
        int height = 0;
        OakRenderPixelFormat fmt = OAK_RENDER_PIX_FMT_RGBA8;
        bool has_depth = false;
        GLuint color_tex = 0;
        GLuint depth_rbo = 0;
    };
    QMap<GLuint, TargetInfo> target_map_;

    struct CachedTexture {
        int width;
        int height;
        int depth;
        OakRenderPixelFormat fmt;
        int channel_count;
        GLuint handle;
        qint64 accessed;
    };
    static const int MAX_TEXTURE_LIFE = 5000;
    static const bool USE_TEXTURE_CACHE = true;
    std::list<CachedTexture> texture_cache_;
    QMutex texture_cache_lock_;

    QMap<GLuint, OakRenderPixelFormat> texture_params_;
};

} // namespace oakgl

#endif // OAKGL_OPENGLRENDERER_H
