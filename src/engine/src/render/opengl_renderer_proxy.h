/***  Oak Video Editor - OpenGL Renderer Proxy  Copyright (C) 2025 mikesolar  ***/

#ifndef OPENGL_RENDERER_PROXY_H
#define OPENGL_RENDERER_PROXY_H

#include "render/renderer.h"
#include "runtime/oak_renderer_runtime.h"

namespace olive
{

class OpenGLRendererProxy : public Renderer {
    Q_OBJECT
public:
    OpenGLRendererProxy(QObject *parent = nullptr);

    virtual ~OpenGLRendererProxy() override;

    virtual bool Init() override;

    virtual void PostDestroy() override;

    virtual void PostInit() override;

    virtual void ClearDestination(olive::Texture *texture = nullptr,
                                  double r = 0.0, double g = 0.0,
                                  double b = 0.0, double a = 0.0) override;

    virtual QVariant CreateNativeShader(olive::ShaderCode code) override;

    virtual void DestroyNativeShader(QVariant shader) override;

    virtual void UploadToTexture(const QVariant &handle,
                                 const VideoParams &params, const void *data,
                                 int linesize) override;

    virtual void DownloadFromTexture(const QVariant &handle,
                                     const VideoParams &params, void *data,
                                     int linesize) override;

    virtual void Flush() override;

    virtual Color GetPixelFromTexture(olive::Texture *texture,
                                      const QPointF &pt) override;

protected:
    virtual void Blit(QVariant shader, olive::AcceleratedJob& job,
                      olive::Texture *destination,
                      olive::VideoParams destination_params,
                      bool clear_destination) override;

    virtual QVariant CreateNativeTexture(int width, int height, int depth,
                                         PixelFormat format, int channel_count,
                                         const void *data = nullptr,
                                         int linesize = 0) override;

    virtual void DestroyNativeTexture(QVariant texture) override;

    virtual void DestroyInternal() override;

private:
    OakRendererHandle handle_ = nullptr;

    OakRenderPixelFormat ToOakPixelFormat(PixelFormat fmt) const;
};

}

#endif // OPENGL_RENDERER_PROXY_H
