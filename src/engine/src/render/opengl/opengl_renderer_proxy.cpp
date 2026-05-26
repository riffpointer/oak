/***  Oak Video Editor - OpenGL Renderer Proxy  Copyright (C) 2025 mikesolar  ***/

#include "opengl_renderer_proxy.h"
#include "runtime/oak_renderer_runtime.h"
#include "render/job/shaderjob.h"
#include "node/value.h"
#include "render/texture.h"

#include <QVector>
#include <QMatrix4x4>

namespace olive {

OpenGLRendererProxy::OpenGLRendererProxy(QObject *parent)
    : Renderer(parent)
    , renderer_(nullptr)
{
}

OpenGLRendererProxy::~OpenGLRendererProxy()
{
    Destroy();
    PostDestroy();
}

bool OpenGLRendererProxy::Init()
{
    auto rt = OakRendererRuntime::Instance();
    if (!rt->Load()) {
        qWarning() << "Failed to load oakgl runtime";
        return false;
    }
    renderer_ = rt->renderer_create("opengl", nullptr);
    return renderer_ != nullptr;
}

void OpenGLRendererProxy::PostDestroy()
{
    if (renderer_) {
        OakRendererRuntime::Instance()->renderer_destroy(renderer_);
        renderer_ = nullptr;
    }
}

void OpenGLRendererProxy::PostInit()
{
    // oakgl internal context is already current after create
}

void OpenGLRendererProxy::ClearDestination(Texture *texture, double r, double g,
                                           double b, double a)
{
    float cc[4] = {static_cast<float>(r), static_cast<float>(g),
                   static_cast<float>(b), static_cast<float>(a)};
    OakTextureHandle tex = nullptr;
    if (texture) {
        tex = reinterpret_cast<OakTextureHandle>(texture->id().value<void*>());
    }
    OakRendererRuntime::Instance()->renderer_clear_texture(renderer_, tex, cc);
}

QVariant OpenGLRendererProxy::CreateNativeShader(ShaderCode code)
{
    auto rt = OakRendererRuntime::Instance();
    QByteArray vert = code.vert_code().toUtf8();
    QByteArray frag = code.frag_code().toUtf8();
    OakShaderHandle sh = rt->shader_compile(renderer_,
        nullptr,
        vert.constData(),
        frag.constData());
    return QVariant::fromValue<void*>(reinterpret_cast<void*>(sh));
}

void OpenGLRendererProxy::DestroyNativeShader(QVariant shader)
{
    auto rt = OakRendererRuntime::Instance();
    OakShaderHandle sh = reinterpret_cast<OakShaderHandle>(shader.value<void*>());
    rt->shader_destroy(renderer_, sh);
}

void OpenGLRendererProxy::UploadToTexture(const QVariant &handle,
                                          const VideoParams &params,
                                          const void *data, int linesize)
{
    auto rt = OakRendererRuntime::Instance();
    OakTextureHandle tex = reinterpret_cast<OakTextureHandle>(handle.value<void*>());
    rt->texture_upload_from_frame(renderer_, params.effective_width(),
                                  params.effective_height(),
                                  ToOakFormat(params.format(), params.channel_count()),
                                  data, linesize);
}

void OpenGLRendererProxy::DownloadFromTexture(const QVariant &handle,
                                              const VideoParams &params,
                                              void *data, int linesize)
{
    auto rt = OakRendererRuntime::Instance();
    OakTextureHandle tex = reinterpret_cast<OakTextureHandle>(handle.value<void*>());
    rt->texture_download(renderer_, tex, params.effective_width(),
                         params.effective_height(),
                         ToOakFormat(params.format(), params.channel_count()),
                         data, linesize);
}

void OpenGLRendererProxy::Flush()
{
    OakRendererRuntime::Instance()->renderer_flush(renderer_);
}

Color OpenGLRendererProxy::GetPixelFromTexture(Texture *texture, const QPointF &pt)
{
    float rgba[4] = {0, 0, 0, 0};
    auto rt = OakRendererRuntime::Instance();
    OakTextureHandle tex = reinterpret_cast<OakTextureHandle>(texture->id().value<void*>());
    rt->renderer_get_pixel(renderer_, tex, static_cast<float>(pt.x()),
                           static_cast<float>(pt.y()), rgba);
    return Color(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void OpenGLRendererProxy::Blit(QVariant shader, AcceleratedJob &a_job,
                               Texture *destination,
                               VideoParams destination_params,
                               bool clear_destination)
{
    auto rt = OakRendererRuntime::Instance();
    OakShaderHandle program = reinterpret_cast<OakShaderHandle>(shader.value<void*>());
    if (!program) return;

    ShaderJob &s_job = dynamic_cast<ShaderJob &>(a_job);
    ShaderJob job(s_job);

    // Collect uniforms and textures
    QVector<OakShaderUniform> uniform_vec;
    QVector<OakTextureHandle> texture_vec;
    QVector<QByteArray> name_storage;
    QMap<QString, int> texture_index_map;

    // Pre-allocate to avoid reallocation invalidating pointers
    int value_count = job.GetValues().size();
    name_storage.reserve(value_count * 3);
    uniform_vec.reserve(value_count * 2);
    texture_vec.reserve(value_count);

    for (auto it = job.GetValues().constBegin();
         it != job.GetValues().constEnd(); ++it) {
        const NodeValue &value = it.value();
        if (value.array()) continue;

        switch (value.type()) {
        case NodeValue::kInt:
        case NodeValue::kBoolean:
        case NodeValue::kCombo: {
            name_storage.append(it.key().toUtf8());
            OakShaderUniform u;
            u.name = name_storage.last().constData();
            u.type = OAK_UNIFORM_INT;
            u.value.i = static_cast<int>(value.toInt());
            uniform_vec.append(u);
            break;
        }
        case NodeValue::kFloat:
        case NodeValue::kRational: {
            name_storage.append(it.key().toUtf8());
            OakShaderUniform u;
            u.name = name_storage.last().constData();
            u.type = OAK_UNIFORM_FLOAT;
            u.value.f = static_cast<float>(value.toDouble());
            uniform_vec.append(u);
            break;
        }
        case NodeValue::kVec2: {
            name_storage.append(it.key().toUtf8());
            OakShaderUniform u;
            u.name = name_storage.last().constData();
            u.type = OAK_UNIFORM_VEC2;
            QVector2D v = value.toVec2();
            u.value.vec2[0] = v.x();
            u.value.vec2[1] = v.y();
            uniform_vec.append(u);
            break;
        }
        case NodeValue::kVec3: {
            name_storage.append(it.key().toUtf8());
            OakShaderUniform u;
            u.name = name_storage.last().constData();
            u.type = OAK_UNIFORM_VEC3;
            QVector3D v = value.toVec3();
            u.value.vec3[0] = v.x();
            u.value.vec3[1] = v.y();
            u.value.vec3[2] = v.z();
            uniform_vec.append(u);
            break;
        }
        case NodeValue::kVec4: {
            name_storage.append(it.key().toUtf8());
            OakShaderUniform u;
            u.name = name_storage.last().constData();
            u.type = OAK_UNIFORM_VEC4;
            QVector4D v = value.toVec4();
            u.value.vec4[0] = v.x();
            u.value.vec4[1] = v.y();
            u.value.vec4[2] = v.z();
            u.value.vec4[3] = v.w();
            uniform_vec.append(u);
            break;
        }
        case NodeValue::kColor: {
            name_storage.append(it.key().toUtf8());
            OakShaderUniform u;
            u.name = name_storage.last().constData();
            u.type = OAK_UNIFORM_VEC4;
            Color c = value.toColor();
            u.value.vec4[0] = c.red();
            u.value.vec4[1] = c.green();
            u.value.vec4[2] = c.blue();
            u.value.vec4[3] = c.alpha();
            uniform_vec.append(u);
            break;
        }
        case NodeValue::kMatrix: {
            name_storage.append(it.key().toUtf8());
            OakShaderUniform u;
            u.name = name_storage.last().constData();
            u.type = OAK_UNIFORM_MAT4;
            QMatrix4x4 m = value.toMatrix();
            for (int i = 0; i < 16; i++) {
                u.value.mat4[i] = m.constData()[i];
            }
            uniform_vec.append(u);
            break;
        }
        case NodeValue::kTexture: {
            TexturePtr texture = value.toTexture();
            int unit = texture_vec.size();
            texture_index_map.insert(it.key(), unit);
            OakTextureHandle handle = texture
                ? reinterpret_cast<OakTextureHandle>(texture->id().value<void*>())
                : nullptr;
            texture_vec.append(handle);

            // Texture uniform
            name_storage.append(it.key().toUtf8());
            OakShaderUniform u;
            u.name = name_storage.last().constData();
            u.type = OAK_UNIFORM_TEXTURE;
            u.texture_unit = unit;
            uniform_vec.append(u);

            // Enabled uniform if shader expects it
            QString enabled_name = QStringLiteral("%1_enabled").arg(it.key());
            name_storage.append(enabled_name.toUtf8());
            OakShaderUniform u_en;
            u_en.name = name_storage.last().constData();
            u_en.type = OAK_UNIFORM_INT;
            u_en.value.i = handle ? 1 : 0;
            uniform_vec.append(u_en);
            break;
        }
        default:
            break;
        }
    }

    // Ensure ove_mvpmat is set
    NodeValue mvpmat_val = job.Get(QStringLiteral("ove_mvpmat"));
    if (mvpmat_val.type() == NodeValue::kMatrix) {
        name_storage.append("ove_mvpmat");
        OakShaderUniform u;
        u.name = name_storage.last().constData();
        u.type = OAK_UNIFORM_MAT4;
        QMatrix4x4 m = mvpmat_val.toMatrix();
        for (int i = 0; i < 16; i++) {
            u.value.mat4[i] = m.constData()[i];
        }
        uniform_vec.append(u);
    }

    // Iterative shader support
    int iteration_count = 1;
    if (job.GetIterationCount() > 1 && !job.GetIterativeInput().isEmpty()) {
        iteration_count = job.GetIterationCount();
    }

    if (iteration_count == 1) {
        if (destination) {
            if (clear_destination) {
                float cc[4] = {0, 0, 0, 0};
                rt->renderer_clear_texture(renderer_,
                    reinterpret_cast<OakTextureHandle>(destination->id().value<void*>()),
                    cc);
            }
            rt->renderer_draw_with_shader_to_texture_ex(
                renderer_, program,
                uniform_vec.constData(), uniform_vec.size(),
                texture_vec.data(), texture_vec.size(),
                reinterpret_cast<OakTextureHandle>(destination->id().value<void*>()));
        } else {
            rt->renderer_draw_with_shader_ex(
                renderer_, program,
                uniform_vec.constData(), uniform_vec.size(),
                texture_vec.data(), texture_vec.size(),
                nullptr);
        }
    } else {
        // Iterative ping-pong: create temporary textures and loop
        TexturePtr output_tex = CreateTexture(destination_params);
        TexturePtr input_tex;
        if (iteration_count > 2) {
            input_tex = CreateTexture(destination_params);
        }

        QString iterative_input = job.GetIterativeInput();
        int iterative_unit = texture_index_map.value(iterative_input, -1);

        for (int iteration = 0; iteration < iteration_count; iteration++) {
            // Set iteration uniform
            QByteArray iter_name_ba = QStringLiteral("ove_iteration").toUtf8();
            OakShaderUniform iter_u;
            iter_u.name = iter_name_ba.constData();
            iter_u.type = OAK_UNIFORM_INT;
            iter_u.value.i = iteration;

            QVector<OakShaderUniform> iter_uniforms = uniform_vec;
            iter_uniforms.append(iter_u);

            if (iteration > 0 && iterative_unit >= 0 && input_tex) {
                // Replace iterative input texture with previous output
                QByteArray iter_input_ba = iterative_input.toUtf8();
                for (int i = 0; i < iter_uniforms.size(); ++i) {
                    if (iter_uniforms[i].type == OAK_UNIFORM_TEXTURE &&
                        strcmp(iter_uniforms[i].name, iter_input_ba.constData()) == 0) {
                        iter_uniforms[i].texture_unit = iterative_unit;
                        break;
                    }
                }
                texture_vec[iterative_unit] = reinterpret_cast<OakTextureHandle>(
                    input_tex->id().value<void*>());
            }

            if (iteration == iteration_count - 1 && destination) {
                if (clear_destination) {
                    float cc[4] = {0, 0, 0, 0};
                    rt->renderer_clear_texture(renderer_,
                        reinterpret_cast<OakTextureHandle>(destination->id().value<void*>()),
                        cc);
                }
                rt->renderer_draw_with_shader_to_texture_ex(
                    renderer_, program,
                    iter_uniforms.constData(), iter_uniforms.size(),
                    texture_vec.data(), texture_vec.size(),
                    reinterpret_cast<OakTextureHandle>(destination->id().value<void*>()));
            } else {
                rt->renderer_draw_with_shader_to_texture_ex(
                    renderer_, program,
                    iter_uniforms.constData(), iter_uniforms.size(),
                    texture_vec.data(), texture_vec.size(),
                    reinterpret_cast<OakTextureHandle>(output_tex->id().value<void*>()));
            }

            std::swap(output_tex, input_tex);
        }
    }
}

QVariant OpenGLRendererProxy::CreateNativeTexture(int width, int height, int depth,
                                                  PixelFormat format, int channel_count,
                                                  const void *data, int linesize)
{
    auto rt = OakRendererRuntime::Instance();
    OakRenderPixelFormat fmt = ToOakFormat(format, channel_count);
    OakTextureHandle tex = nullptr;
    if (data) {
        tex = rt->texture_upload_from_frame(renderer_, width, height, fmt,
                                            data, linesize);
    } else {
        // For empty texture, use texture_upload with zero data
        tex = rt->texture_upload(renderer_, width, height, fmt,
                                 nullptr, 0,
                                 OAK_FILTER_LINEAR, OAK_WRAP_CLAMP);
    }
    return QVariant::fromValue<void*>(reinterpret_cast<void*>(tex));
}

void OpenGLRendererProxy::DestroyNativeTexture(QVariant texture)
{
    auto rt = OakRendererRuntime::Instance();
    OakTextureHandle tex = reinterpret_cast<OakTextureHandle>(texture.value<void*>());
    rt->texture_destroy(renderer_, tex);
}

void OpenGLRendererProxy::DestroyInternal()
{
    // Nothing extra to destroy; renderer lifetime is managed by PostDestroy
}

OakRenderPixelFormat OpenGLRendererProxy::ToOakFormat(PixelFormat fmt, int channels)
{
    switch (fmt) {
    case PixelFormat::U8:
        switch (channels) {
        case 1: return OAK_RENDER_PIX_FMT_R8;
        case 2: return OAK_RENDER_PIX_FMT_RG8;
        default: return OAK_RENDER_PIX_FMT_RGBA8;
        }
    case PixelFormat::U16:
        return OAK_RENDER_PIX_FMT_RGBA16;
    case PixelFormat::F32:
        return OAK_RENDER_PIX_FMT_RGBA32F;
    default:
        return OAK_RENDER_PIX_FMT_RGBA8;
    }
}

} // namespace olive
