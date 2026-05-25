#ifndef OPENGL_RENDERER_PROXY_H
#define OPENGL_RENDERER_PROXY_H

#include "render/renderer.h"
#include "oak/renderer_api.h"

#ifdef __APPLE__
#include <dlfcn.h>
#endif

namespace olive {

class OpenGLRendererProxy : public Renderer {
    Q_OBJECT
public:
    OpenGLRendererProxy(QObject *parent = nullptr);
    ~OpenGLRendererProxy() override;

    bool Init() override;
    void PostDestroy() override;
    void PostInit() override;
    void ClearDestination(Texture *texture, double r, double g, double b, double a) override;
    QVariant CreateNativeShader(ShaderCode code) override;
    void DestroyNativeShader(QVariant shader) override;
    void UploadToTexture(const QVariant &handle, const VideoParams &params, const void *data, int linesize) override;
    void DownloadFromTexture(const QVariant &handle, const VideoParams &params, void *data, int linesize) override;
    void Flush() override;
    Color GetPixelFromTexture(Texture *texture, const QPointF &pt) override;

protected:
    void Blit(QVariant shader, AcceleratedJob &job, Texture *destination,
              VideoParams destination_params, bool clear_destination) override;
    QVariant CreateNativeTexture(int width, int height, int depth,
                                 PixelFormat format, int channel_count,
                                 const void *data, int linesize) override;
    void DestroyNativeTexture(QVariant texture) override;
    void DestroyInternal() override;

private:
    bool LoadLibrary();
    template <typename T>
    T LoadSym(const char *name);
    static OakRenderPixelFormat ToOakFormat(PixelFormat fmt, int channels);

    void *so_handle_ = nullptr;
    OakRendererHandle renderer_ = nullptr;

    // C API function pointers
    OakRendererHandle (*fp_create_)(const char*, void*) = nullptr;
    void (*fp_destroy_)(OakRendererHandle) = nullptr;
    OakTextureHandle (*fp_tex_upload_)(OakRendererHandle, int, int, OakRenderPixelFormat, const void*, size_t, int, int) = nullptr;
    OakTextureHandle (*fp_tex_upload_frame_)(OakRendererHandle, int, int, OakRenderPixelFormat, const void*, int) = nullptr;
    void (*fp_tex_destroy_)(OakRendererHandle, OakTextureHandle) = nullptr;
    OakTargetHandle (*fp_target_create_)(OakRendererHandle, int, int, OakRenderPixelFormat, bool) = nullptr;
    void (*fp_target_destroy_)(OakRendererHandle, OakTargetHandle) = nullptr;
    OakTextureHandle (*fp_target_detach_)(OakRendererHandle, OakTargetHandle) = nullptr;
    OakShaderHandle (*fp_shader_compile_)(OakRendererHandle, const char*, const char*, const char*) = nullptr;
    void (*fp_shader_destroy_)(OakRendererHandle, OakShaderHandle) = nullptr;
    void (*fp_draw_shader_)(OakRendererHandle, OakShaderHandle, const char*, OakTextureHandle*, int, OakTargetHandle) = nullptr;
    void (*fp_draw_shader_tex_)(OakRendererHandle, OakShaderHandle, const char*, OakTextureHandle*, int, OakTextureHandle) = nullptr;
    void (*fp_begin_)(OakRendererHandle, OakTargetHandle, const float*) = nullptr;
    void (*fp_end_)(OakRendererHandle) = nullptr;
    int (*fp_readback_)(OakRendererHandle, OakTargetHandle, OakRenderPixelFormat, void**, int*) = nullptr;
    void (*fp_free_readback_)(void*) = nullptr;
};

} // namespace olive

#endif
