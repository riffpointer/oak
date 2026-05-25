/***

  Oak Video Editor - OpenGL Render Backend Implementation
  Copyright (C) 2025 mikesolar

***/

#include "openglrenderer.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLExtraFunctions>
#include <QRegularExpression>
#include <QThread>
#include <iostream>

namespace oakgl
{

/* ================================================================ */
/*  Built-in shader sources (embedded, no external resource files)   */
/* ================================================================ */

static const char *kDefaultVert =
    "uniform mat4 ove_mvpmat;\n"
    "in vec4 a_position;\n"
    "in vec2 a_texcoord;\n"
    "out vec2 ove_texcoord;\n"
    "void main() {\n"
    "    gl_Position = ove_mvpmat * a_position;\n"
    "    ove_texcoord = a_texcoord;\n"
    "}\n";

static const char *kDefaultFrag =
    "uniform sampler2D ove_maintex;\n"
    "in vec2 ove_texcoord;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(ove_maintex, ove_texcoord);\n"
    "}\n";

static const char *kSolidFrag =
    "in vec2 ove_texcoord;\n"
    "out vec4 frag_color;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "    frag_color = u_color;\n"
    "}\n";

static const char *kYUVBlitVert =
    "in vec2 a_position;\n"
    "in vec2 a_texcoord;\n"
    "out vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

static const char *kYUVBlitFrag =
    "in vec2 v_texcoord;\n"
    "out vec4 frag_color;\n"
    "uniform sampler2D u_yTex;\n"
    "uniform sampler2D u_uTex;\n"
    "uniform sampler2D u_vTex;\n"
    "uniform mat3 u_colorMatrix;\n"
    "uniform bool u_fullRange;\n"
    "void main() {\n"
    "    float y = texture(u_yTex, v_texcoord).r;\n"
    "    float u = texture(u_uTex, v_texcoord).r;\n"
    "    float v = texture(u_vTex, v_texcoord).r;\n"
    "    vec3 yuv;\n"
    "    if (u_fullRange) {\n"
    "        yuv = vec3(y, u - 0.5, v - 0.5);\n"
    "    } else {\n"
    "        yuv = vec3(\n"
    "            (y - 16.0/255.0) * (255.0/219.0),\n"
    "            (u - 128.0/255.0) * (255.0/224.0),\n"
    "            (v - 128.0/255.0) * (255.0/224.0)\n"
    "        );\n"
    "    }\n"
    "    vec3 rgb = u_colorMatrix * yuv;\n"
    "    frag_color = vec4(rgb, 1.0);\n"
    "}\n";

static const QVector<GLfloat> kBlitVertices = {
    -1.0f, -1.0f, 0.0f,  1.0f, -1.0f, 0.0f,  1.0f, 1.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, -1.0f,  1.0f, 0.0f,  1.0f, 1.0f, 0.0f
};

static const QVector<GLfloat> kBlitTexcoords = {
    0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
    0.0f, 0.0f,  0.0f, 1.0f,  1.0f, 1.0f
};

/* ================================================================ */
/*  Helpers                                                          */
/* ================================================================ */

class ErrorPrinter {
public:
    ErrorPrinter(const char *name, QOpenGLFunctions *f)
    {
        GLuint err = f->glGetError();
        if (err > 0)
            qDebug() << name << "entered with" << err;
        name_ = name;
        functions_ = f;
    }
    ~ErrorPrinter()
    {
        GLuint err = functions_->glGetError();
        if (err > 0)
            qDebug() << name_ << "exited with" << err;
    }
private:
    const char *name_;
    QOpenGLFunctions *functions_;
};

#define PRINT_GL_ERRORS ErrorPrinter __e(__FUNCTION__, functions_)

/* ================================================================ */
/*  Constructor / Destructor                                         */
/* ================================================================ */

OpenGLRenderer::OpenGLRenderer(QObject *parent)
    : QObject(parent)
    , context_(nullptr)
    , functions_(nullptr)
    , framebuffer_(0)
{
}

OpenGLRenderer::~OpenGLRenderer()
{
    Destroy();
}

/* ================================================================ */
/*  Context lifecycle                                                */
/* ================================================================ */

bool OpenGLRenderer::Init()
{
    if (context_) {
        qCritical() << "Can't initialize already initialized OpenGLRenderer";
        return false;
    }

    surface_.create();

    context_ = new QOpenGLContext(this);
    context_->setShareContext(QOpenGLContext::globalShareContext());
    if (!context_->create()) {
        qCritical() << "Failed to create OpenGL context";
        return false;
    }

    context_->moveToThread(this->thread());

    // Make context current on that surface
    if (!context_->makeCurrent(&surface_)) {
        qCritical() << "Failed to makeCurrent() on offscreen surface in thread" << thread();
        return false;
    }

    functions_ = context_->functions();
    functions_->glBlendFunc(GL_ONE, GL_ZERO);
    functions_->glGenFramebuffers(1, &framebuffer_);

    return true;
}

void OpenGLRenderer::Destroy()
{
    if (context_) {
        if (functions_ && framebuffer_) {
            functions_->glDeleteFramebuffers(1, &framebuffer_);
        }
        framebuffer_ = 0;

        // Destroy cached textures
        for (auto it = texture_cache_.begin(); it != texture_cache_.end(); ++it) {
            functions_->glDeleteTextures(1, &it->handle);
        }
        texture_cache_.clear();
        texture_params_.clear();

        // Destroy targets
        for (auto it = target_map_.begin(); it != target_map_.end(); ++it) {
            DestroyTarget(it.key());
        }
        target_map_.clear();

        if (context_->parent() == this) {
            delete context_;
        }
        context_ = nullptr;
        functions_ = nullptr;
    }

    if (surface_.isValid()) {
        surface_.destroy();
    }
}

bool OpenGLRenderer::EnsureContextCurrent(const char *caller)
{
    if (!context_) {
        qWarning() << caller << "called without an OpenGL context";
        return false;
    }

    if (QOpenGLContext::currentContext() != context_) {
        if (context_->parent() == this && surface_.isValid()) {
            if (!context_->makeCurrent(&surface_)) {
                qWarning() << caller << "failed to make context current";
                return false;
            }
        } else {
            qWarning() << caller << "OpenGL context not current";
            return false;
        }
    }

    if (!functions_) {
        functions_ = context_->functions();
    }

    if (!functions_) {
        qWarning() << caller << "OpenGL functions not available";
        return false;
    }

    if (!framebuffer_) {
        functions_->glGenFramebuffers(1, &framebuffer_);
    }

    return true;
}

/* ================================================================ */
/*  Pixel format mapping                                             */
/* ================================================================ */

GLint OpenGLRenderer::GetInternalFormat(OakRenderPixelFormat fmt, int channels)
{
    switch (fmt) {
    case OAK_RENDER_PIX_FMT_RGBA8:
        switch (channels) {
        case 1: return GL_R8;
        case 2: return GL_RG8;
        case 3: return GL_RGB8;
        case 4: return GL_RGBA8;
        }
        break;
    case OAK_RENDER_PIX_FMT_RGBA16:
        switch (channels) {
        case 1: return GL_R16;
        case 2: return GL_RG16;
        case 3: return GL_RGB16;
        case 4: return GL_RGBA16;
        }
        break;
    case OAK_RENDER_PIX_FMT_RGBA32F:
        switch (channels) {
        case 1: return GL_R32F;
        case 2: return GL_RG32F;
        case 3: return GL_RGB32F;
        case 4: return GL_RGBA32F;
        }
        break;
    case OAK_RENDER_PIX_FMT_R8:
        return GL_R8;
    case OAK_RENDER_PIX_FMT_RG8:
        return GL_RG8;
    }
    return GL_INVALID_VALUE;
}

GLenum OpenGLRenderer::GetPixelType(OakRenderPixelFormat fmt)
{
    switch (fmt) {
    case OAK_RENDER_PIX_FMT_RGBA8:
    case OAK_RENDER_PIX_FMT_R8:
    case OAK_RENDER_PIX_FMT_RG8:
        return GL_UNSIGNED_BYTE;
    case OAK_RENDER_PIX_FMT_RGBA16:
        return GL_UNSIGNED_SHORT;
    case OAK_RENDER_PIX_FMT_RGBA32F:
        return GL_FLOAT;
    }
    return GL_INVALID_VALUE;
}

GLenum OpenGLRenderer::GetPixelFormat(int channels)
{
    switch (channels) {
    case 1: return GL_RED;
    case 2: return GL_RG;
    case 3: return GL_RGB;
    case 4: return GL_RGBA;
    default: return GL_INVALID_VALUE;
    }
}

/* ================================================================ */
/*  Texture                                                          */
/* ================================================================ */

GLuint OpenGLRenderer::CreateTexture(int width, int height, int depth,
                                     int channels, OakRenderPixelFormat fmt,
                                     const void *data, int linesize)
{
    if (!EnsureContextCurrent(__FUNCTION__)) {
        return 0;
    }

    GLuint texture = 0;

    // Try texture cache
    if (USE_TEXTURE_CACHE) {
        QMutexLocker locker(&texture_cache_lock_);
        for (auto it = texture_cache_.begin(); it != texture_cache_.end(); ++it) {
            if (it->width == width && it->height == height && it->depth == depth &&
                it->fmt == fmt && it->channel_count == channels) {
                texture = it->handle;
                texture_cache_.erase(it);
                break;
            }
        }
    }

    bool is_3d = depth > 1;
    GLenum target = is_3d ? GL_TEXTURE_3D : GL_TEXTURE_2D;
    GLenum target_binding = is_3d ? GL_TEXTURE_BINDING_3D : GL_TEXTURE_BINDING_2D;

    if (texture == 0) {
        functions_->glGenTextures(1, &texture);
        texture_params_.insert(texture, fmt);

        GLint current_tex;
        functions_->glGetIntegerv(target_binding, &current_tex);
        functions_->glBindTexture(target, texture);

        GLint internal_fmt = GetInternalFormat(fmt, channels);
        GLenum pixel_fmt = GetPixelFormat(channels);
        GLenum pixel_type = GetPixelType(fmt);

        if (is_3d) {
            context_->extraFunctions()->glTexImage3D(
                target, 0, internal_fmt, width, height, depth,
                0, pixel_fmt, pixel_type, data);
        } else {
            functions_->glTexImage2D(
                target, 0, internal_fmt, width, height,
                0, pixel_fmt, pixel_type, data);
        }

        functions_->glBindTexture(target, current_tex);
    } else if (data) {
        UploadTexture(texture, width, height, depth, channels, fmt, data, linesize);
    } else {
        Flush();
    }

    return texture;
}

void OpenGLRenderer::DestroyTexture(GLuint tex)
{
    if (tex == 0 || !functions_) return;

    if (USE_TEXTURE_CACHE) {
        texture_cache_lock_.lock();
        auto it = texture_params_.find(tex);
        if (it != texture_params_.end()) {
            OakRenderPixelFormat fmt = it.value();
            // We don't store exact params in the cache key, use defaults
            texture_cache_.push_back({0, 0, 1, fmt, 4, tex,
                                      QDateTime::currentMSecsSinceEpoch()});
        }
        texture_cache_lock_.unlock();

        if (QThread::currentThread() == this->thread()) {
            QMutexLocker locker(&texture_cache_lock_);
            for (auto it = texture_cache_.begin(); it != texture_cache_.end();) {
                if (it->accessed < QDateTime::currentMSecsSinceEpoch() - MAX_TEXTURE_LIFE) {
                    functions_->glDeleteTextures(1, &it->handle);
                    it = texture_cache_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    } else {
        functions_->glDeleteTextures(1, &tex);
    }
    texture_params_.remove(tex);
}

void OpenGLRenderer::UploadTexture(GLuint tex, int width, int height, int depth,
                                   int channels, OakRenderPixelFormat fmt,
                                   const void *data, int linesize)
{
    if (!functions_ || tex == 0) return;

    bool is_3d = depth > 1;
    GLenum tex_type = is_3d ? GL_TEXTURE_3D : GL_TEXTURE_2D;
    GLenum tex_binding = is_3d ? GL_TEXTURE_BINDING_3D : GL_TEXTURE_BINDING_2D;

    GLint current_tex;
    functions_->glGetIntegerv(tex_binding, &current_tex);
    functions_->glBindTexture(tex_type, tex);
    functions_->glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize);

    GLenum pixel_fmt = GetPixelFormat(channels);
    GLenum pixel_type = GetPixelType(fmt);

    if (!is_3d) {
        functions_->glTexSubImage2D(tex_type, 0, 0, 0, width, height,
                                    pixel_fmt, pixel_type, data);
    } else {
        context_->extraFunctions()->glTexSubImage3D(
            tex_type, 0, 0, 0, 0, width, height, depth,
            pixel_fmt, pixel_type, data);
    }

    functions_->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    functions_->glBindTexture(tex_type, current_tex);
}

void OpenGLRenderer::DownloadTexture(GLuint tex, int width, int height, int depth,
                                     int channels, OakRenderPixelFormat fmt,
                                     void *data, int linesize)
{
    if (!EnsureContextCurrent(__FUNCTION__)) return;
    if (tex == 0 || !functions_->glIsTexture(tex)) {
        qWarning() << "DownloadTexture called with invalid texture";
        return;
    }

    (void)depth; // 2D download only for now

    GLint current_tex;
    functions_->glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_tex);

    AttachTextureAsDestination(tex);

    GLenum status = functions_->glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qWarning() << "DownloadTexture framebuffer incomplete" << status;
        DetachTextureAsDestination();
        return;
    }

    functions_->glPixelStorei(GL_PACK_ROW_LENGTH, linesize);
    functions_->glFinish();

    GLenum pixel_fmt = GetPixelFormat(channels);
    GLenum pixel_type = GetPixelType(fmt);
    functions_->glReadPixels(0, 0, width, height, pixel_fmt, pixel_type, data);

    functions_->glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    DetachTextureAsDestination();
    functions_->glBindTexture(GL_TEXTURE_2D, current_tex);
}

/* ================================================================ */
/*  Render Target                                                    */
/* ================================================================ */

GLuint OpenGLRenderer::CreateTarget(int width, int height,
                                    OakRenderPixelFormat fmt, bool has_depth)
{
    if (!EnsureContextCurrent(__FUNCTION__)) return 0;

    GLuint fbo = 0;
    functions_->glGenFramebuffers(1, &fbo);
    functions_->glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create color attachment texture
    GLuint color_tex = CreateTexture(width, height, 1, 4, fmt, nullptr, 0);
    functions_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, color_tex, 0);

    GLuint depth_rbo = 0;
    if (has_depth) {
        functions_->glGenRenderbuffers(1, &depth_rbo);
        functions_->glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
        functions_->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                                          width, height);
        functions_->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                              GL_RENDERBUFFER, depth_rbo);
        functions_->glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    GLenum status = functions_->glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qWarning() << "CreateTarget framebuffer incomplete" << status;
        DestroyTarget(fbo);
        return 0;
    }

    TargetInfo info;
    info.width = width;
    info.height = height;
    info.fmt = fmt;
    info.has_depth = has_depth;
    info.color_tex = color_tex;
    info.depth_rbo = depth_rbo;
    target_map_.insert(fbo, info);

    // Restore default framebuffer
    GLuint default_fbo = context_ ? context_->defaultFramebufferObject() : 0;
    functions_->glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);

    return fbo;
}

GLuint OpenGLRenderer::GetTargetColorTexture(GLuint fbo)
{
    auto it = target_map_.find(fbo);
    if (it != target_map_.end()) {
        return it->color_tex;
    }
    return 0;
}

GLuint OpenGLRenderer::DetachTargetColorTexture(GLuint fbo)
{
    auto it = target_map_.find(fbo);
    if (it != target_map_.end()) {
        GLuint tex = it->color_tex;
        it->color_tex = 0;
        return tex;
    }
    return 0;
}

bool OpenGLRenderer::GetTargetSize(GLuint fbo, int* out_width, int* out_height)
{
    auto it = target_map_.find(fbo);
    if (it != target_map_.end()) {
        if (out_width)  *out_width  = it->width;
        if (out_height) *out_height = it->height;
        return true;
    }
    return false;
}

void OpenGLRenderer::DestroyTarget(GLuint fbo)
{
    if (fbo == 0 || !functions_) return;

    auto it = target_map_.find(fbo);
    if (it != target_map_.end()) {
        const TargetInfo &info = it.value();
        if (info.color_tex) {
            DestroyTexture(info.color_tex);
        }
        if (info.depth_rbo) {
            functions_->glDeleteRenderbuffers(1, &info.depth_rbo);
        }
        target_map_.erase(it);
    }

    functions_->glDeleteFramebuffers(1, &fbo);
}

void OpenGLRenderer::ResizeTarget(GLuint fbo, int width, int height)
{
    auto it = target_map_.find(fbo);
    if (it == target_map_.end()) return;

    TargetInfo &info = it.value();
    info.width = width;
    info.height = height;

    // Recreate color attachment
    if (info.color_tex) {
        DestroyTexture(info.color_tex);
    }
    info.color_tex = CreateTexture(width, height, 1, 4, info.fmt, nullptr, 0);

    functions_->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    functions_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, info.color_tex, 0);

    if (info.has_depth && info.depth_rbo) {
        functions_->glBindRenderbuffer(GL_RENDERBUFFER, info.depth_rbo);
        functions_->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                                          width, height);
        functions_->glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    functions_->glBindFramebuffer(GL_FRAMEBUFFER,
                                  context_ ? context_->defaultFramebufferObject() : 0);
}

/* ================================================================ */
/*  Shader                                                           */
/* ================================================================ */

GLuint OpenGLRenderer::CompileShaderInternal(GLenum type, const QString &code)
{
    const bool is_gles = context_ && context_->isOpenGLES();
    const int major = context_ ? context_->format().majorVersion() : 0;
    const int minor = context_ ? context_->format().minorVersion() : 0;
    const bool is_gles2 = is_gles && (major < 3);

    const QString gles_preamble = is_gles2
        ? QStringLiteral("#version 100\n\nprecision highp float;\n\n#define frag_color gl_FragColor\n")
        : QStringLiteral("#version 300 es\n\nprecision highp float;\n\n");
    const QString desktop_preamble = QStringLiteral("#version 150\n\nprecision highp float;\n\n");
    const QString shader_preamble = is_gles ? gles_preamble : desktop_preamble;

    QString base_code = code;
    if (base_code.isEmpty()) {
        if (type == GL_FRAGMENT_SHADER) {
            base_code = QString::fromUtf8(kDefaultFrag);
        } else if (type == GL_VERTEX_SHADER) {
            base_code = QString::fromUtf8(kDefaultVert);
        }
    }

    QString complete_code;
    if (base_code.startsWith(QStringLiteral("#version"))) {
        if (is_gles || !desktop_preamble.startsWith(QStringLiteral("#version"))) {
            int newline = base_code.indexOf('\n');
            if (newline >= 0) {
                complete_code = shader_preamble + base_code.mid(newline + 1);
            } else {
                complete_code = shader_preamble;
            }
        } else {
            complete_code = base_code;
        }
    } else {
        complete_code = shader_preamble + base_code;
    }

    if (is_gles2) {
        if (type == GL_VERTEX_SHADER) {
            complete_code.replace(QRegularExpression(QStringLiteral("\\bin\\b")),
                                  QStringLiteral("attribute"));
            complete_code.replace(QRegularExpression(QStringLiteral("\\bout\\b")),
                                  QStringLiteral("varying"));
        } else if (type == GL_FRAGMENT_SHADER) {
            complete_code.replace(QRegularExpression(QStringLiteral("\\bin\\b")),
                                  QStringLiteral("varying"));
            complete_code.replace(QRegularExpression(
                                      QStringLiteral("\\bout\\s+vec4\\s+frag_color\\s*;")),
                                  QStringLiteral("// frag_color output"));
            complete_code.replace(QRegularExpression(QStringLiteral("\\btexture\\b")),
                                  QStringLiteral("texture2D"));
        }
    }

    QByteArray code_utf8 = complete_code.toUtf8();
    const char *code_cstr = code_utf8.constData();

    GLuint shader = functions_->glCreateShader(type);
    functions_->glShaderSource(shader, 1, &code_cstr, nullptr);
    functions_->glCompileShader(shader);

    GLint success;
    functions_->glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        qWarning() << "Failed to compile OpenGL shader";
        QByteArray error_log(10240, Qt::Uninitialized);
        functions_->glGetShaderInfoLog(shader, error_log.size(), nullptr, error_log.data());
        std::cout << error_log.constData() << std::endl << code_cstr << std::endl;
        functions_->glDeleteShader(shader);
        shader = 0;
    }

    return shader;
}

GLuint OpenGLRenderer::CompileShader(const char *vert_src, const char *frag_src)
{
    if (!EnsureContextCurrent(__FUNCTION__)) return 0;

    PRINT_GL_ERRORS;

    GLuint vert = CompileShaderInternal(GL_VERTEX_SHADER,
                                        vert_src ? QString::fromUtf8(vert_src) : QString());
    GLuint frag = CompileShaderInternal(GL_FRAGMENT_SHADER,
                                        frag_src ? QString::fromUtf8(frag_src) : QString());

    GLuint program = 0;
    if (frag && vert) {
        program = functions_->glCreateProgram();
        functions_->glAttachShader(program, frag);
        functions_->glAttachShader(program, vert);
        functions_->glLinkProgram(program);

        GLint success;
        functions_->glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            qWarning() << "Failed to link OpenGL shader program";
            functions_->glDeleteProgram(program);
            program = 0;
        }
    }

    functions_->glDeleteShader(frag);
    functions_->glDeleteShader(vert);

    return program;
}

void OpenGLRenderer::DestroyShader(GLuint program)
{
    if (!functions_ || program == 0) return;
    functions_->glDeleteProgram(program);
}

/* ================================================================ */
/*  FBO attachment helpers                                           */
/* ================================================================ */

void OpenGLRenderer::AttachTextureAsDestination(GLuint texture)
{
    functions_->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    functions_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, texture, 0);
}

void OpenGLRenderer::DetachTextureAsDestination()
{
    GLuint default_fbo = context_ ? context_->defaultFramebufferObject() : 0;
    functions_->glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
}

/* ================================================================ */
/*  Drawing                                                          */
/* ================================================================ */

void OpenGLRenderer::ClearDestinationInternal(float r, float g, float b, float a)
{
    functions_->glClearColor(r, g, b, a);
    functions_->glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderer::BeginFrame(GLuint fbo, const float *clear_color)
{
    if (!EnsureContextCurrent(__FUNCTION__)) return;

    if (fbo) {
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    } else {
        GLuint default_fbo = context_ ? context_->defaultFramebufferObject() : 0;
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
    }

    if (clear_color) {
        ClearDestinationInternal(clear_color[0], clear_color[1],
                                 clear_color[2], clear_color[3]);
    }
}

void OpenGLRenderer::EndFrame()
{
    // Nothing to do for OpenGL, but could add fence/synchronization here
}

void OpenGLRenderer::ClearTexture(GLuint tex, const float *clear_color)
{
    if (!functions_) return;
    EnsureContextCurrent("ClearTexture");
    if (tex) {
        AttachTextureAsDestination(tex);
    }
    if (clear_color) {
        ClearDestinationInternal(clear_color[0], clear_color[1],
                                 clear_color[2], clear_color[3]);
    }
    if (tex) {
        DetachTextureAsDestination();
    }
}

void OpenGLRenderer::PrepareInputTexture(GLenum target, OakFilterMode filter)
{
    switch (filter) {
    case OAK_FILTER_NEAREST:
        functions_->glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        functions_->glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        break;
    case OAK_FILTER_LINEAR:
        functions_->glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        functions_->glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;
    case OAK_FILTER_MIPMAP_LINEAR:
        functions_->glGenerateMipmap(target);
        functions_->glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
                                    GL_LINEAR_MIPMAP_LINEAR);
        functions_->glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;
    }

    functions_->glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    functions_->glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (target == GL_TEXTURE_3D) {
        functions_->glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
}

void OpenGLRenderer::DrawQuad(const float *mvp, GLuint tex,
                              OakBlendMode blend, const float *color,
                              const float *uv_rect)
{
    PRINT_GL_ERRORS;

    // Set blend mode
    switch (blend) {
    case OAK_BLEND_REPLACE:
        functions_->glEnable(GL_BLEND);
        functions_->glBlendFunc(GL_ONE, GL_ZERO);
        break;
    case OAK_BLEND_OVER:
        functions_->glEnable(GL_BLEND);
        functions_->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case OAK_BLEND_ADD:
        functions_->glEnable(GL_BLEND);
        functions_->glBlendFunc(GL_ONE, GL_ONE);
        break;
    case OAK_BLEND_MULTIPLY:
        functions_->glEnable(GL_BLEND);
        functions_->glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;
    case OAK_BLEND_SCREEN:
        functions_->glEnable(GL_BLEND);
        functions_->glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
        break;
    case OAK_BLEND_SUBTRACT:
        functions_->glEnable(GL_BLEND);
        functions_->glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        break;
    }

    // Build a simple shader if none provided
    GLuint program = 0;
    if (tex) {
        program = CompileShader(kDefaultVert, kDefaultFrag);
    } else {
        program = CompileShader(kDefaultVert, kSolidFrag);
    }

    if (program == 0) {
        functions_->glDisable(GL_BLEND);
        return;
    }

    functions_->glUseProgram(program);

    // Set MVP matrix
    GLint mvp_loc = functions_->glGetUniformLocation(program, "ove_mvpmat");
    if (mvp_loc > -1 && mvp) {
        functions_->glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp);
    }

    // Set texture
    if (tex) {
        GLint tex_loc = functions_->glGetUniformLocation(program, "ove_maintex");
        if (tex_loc > -1) {
            functions_->glUniform1i(tex_loc, 0);
            functions_->glActiveTexture(GL_TEXTURE0);
            functions_->glBindTexture(GL_TEXTURE_2D, tex);
            PrepareInputTexture(GL_TEXTURE_2D, OAK_FILTER_LINEAR);
        }
    }

    // Set solid color
    if (!tex && color) {
        GLint col_loc = functions_->glGetUniformLocation(program, "u_color");
        if (col_loc > -1) {
            functions_->glUniform4f(col_loc, color[0], color[1], color[2], color[3]);
        }
    }

    // Set viewport
    GLint vp[4];
    functions_->glGetIntegerv(GL_VIEWPORT, vp);

    // Bind VAO and VBOs
    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();

    QOpenGLBuffer vert_vbo(QOpenGLBuffer::VertexBuffer);
    vert_vbo.create();
    vert_vbo.bind();
    vert_vbo.allocate(kBlitVertices.constData(),
                      kBlitVertices.size() * sizeof(GLfloat));
    vert_vbo.release();

    QOpenGLBuffer tex_vbo(QOpenGLBuffer::VertexBuffer);
    tex_vbo.create();
    tex_vbo.bind();

    // Use custom UV rect if provided
    QVector<GLfloat> uvs = kBlitTexcoords;
    if (uv_rect) {
        uvs = {
            uv_rect[0], uv_rect[1],  uv_rect[2], uv_rect[1],  uv_rect[2], uv_rect[3],
            uv_rect[0], uv_rect[1],  uv_rect[0], uv_rect[3],  uv_rect[2], uv_rect[3]
        };
    }
    tex_vbo.allocate(uvs.constData(), uvs.size() * sizeof(GLfloat));
    tex_vbo.release();

    GLint vertex_loc = functions_->glGetAttribLocation(program, "a_position");
    if (vertex_loc != -1) {
        vert_vbo.bind();
        functions_->glEnableVertexAttribArray(vertex_loc);
        functions_->glVertexAttribPointer(vertex_loc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        vert_vbo.release();
    }

    GLint texcoord_loc = functions_->glGetAttribLocation(program, "a_texcoord");
    if (texcoord_loc != -1) {
        tex_vbo.bind();
        functions_->glEnableVertexAttribArray(texcoord_loc);
        functions_->glVertexAttribPointer(texcoord_loc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        tex_vbo.release();
    }

    functions_->glDrawArrays(GL_TRIANGLES, 0, kBlitVertices.size() / 3);

    // Cleanup
    functions_->glUseProgram(0);
    functions_->glActiveTexture(GL_TEXTURE0);
    functions_->glBindTexture(GL_TEXTURE_2D, 0);
    functions_->glDisable(GL_BLEND);

    tex_vbo.destroy();
    vert_vbo.destroy();
    vao.release();
    vao.destroy();

    DestroyShader(program);
}

void OpenGLRenderer::DrawLines(const float *points, int point_count,
                               const float *color, float line_width)
{
    if (!points || point_count < 2) return;

    // Simple line shader
    const char *vert =
        "uniform mat4 ove_mvpmat;\n"
        "in vec2 a_position;\n"
        "void main() { gl_Position = ove_mvpmat * vec4(a_position, 0.0, 1.0); }\n";
    const char *frag =
        "out vec4 frag_color;\n"
        "uniform vec4 u_color;\n"
        "void main() { frag_color = u_color; }\n";

    GLuint program = CompileShader(vert, frag);
    if (!program) return;

    functions_->glUseProgram(program);

    GLint col_loc = functions_->glGetUniformLocation(program, "u_color");
    if (col_loc > -1 && color) {
        functions_->glUniform4f(col_loc, color[0], color[1], color[2], color[3]);
    }

    functions_->glLineWidth(line_width);

    QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
    vbo.create();
    vbo.bind();
    vbo.allocate(points, point_count * 2 * sizeof(float));
    vbo.release();

    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();

    GLint pos_loc = functions_->glGetAttribLocation(program, "a_position");
    if (pos_loc != -1) {
        vbo.bind();
        functions_->glEnableVertexAttribArray(pos_loc);
        functions_->glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        vbo.release();
    }

    functions_->glDrawArrays(GL_LINES, 0, point_count);

    functions_->glUseProgram(0);
    vbo.destroy();
    vao.release();
    vao.destroy();
    DestroyShader(program);
}

void OpenGLRenderer::DrawPolygon(const float *points, int point_count,
                                 const float *color)
{
    if (!points || point_count < 3) return;

    const char *vert =
        "uniform mat4 ove_mvpmat;\n"
        "in vec2 a_position;\n"
        "void main() { gl_Position = ove_mvpmat * vec4(a_position, 0.0, 1.0); }\n";
    const char *frag =
        "out vec4 frag_color;\n"
        "uniform vec4 u_color;\n"
        "void main() { frag_color = u_color; }\n";

    GLuint program = CompileShader(vert, frag);
    if (!program) return;

    functions_->glUseProgram(program);

    GLint col_loc = functions_->glGetUniformLocation(program, "u_color");
    if (col_loc > -1 && color) {
        functions_->glUniform4f(col_loc, color[0], color[1], color[2], color[3]);
    }

    QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
    vbo.create();
    vbo.bind();
    vbo.allocate(points, point_count * 2 * sizeof(float));
    vbo.release();

    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();

    GLint pos_loc = functions_->glGetAttribLocation(program, "a_position");
    if (pos_loc != -1) {
        vbo.bind();
        functions_->glEnableVertexAttribArray(pos_loc);
        functions_->glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        vbo.release();
    }

    functions_->glDrawArrays(GL_TRIANGLE_FAN, 0, point_count);

    functions_->glUseProgram(0);
    vbo.destroy();
    vao.release();
    vao.destroy();
    DestroyShader(program);
}

void OpenGLRenderer::ApplyEffect(const char *effect_name, const char *params,
                                 GLuint src_fbo, GLuint dst_fbo)
{
    (void)params;
    if (!effect_name) return;

    QString name = QString::fromUtf8(effect_name);

    // TODO: implement built-in effects (gaussian blur, box blur, gamma correction)
    // For now, just blit from src to dst
    if (src_fbo && dst_fbo) {
        // Copy framebuffer content
        // This is a stub - real implementation would compile effect-specific shaders
    }
}

void OpenGLRenderer::DrawWithShader(GLuint program, const char *uniforms_json,
                                    GLuint *textures, int tex_count, GLuint dst_fbo)
{
    if (!program) return;

    if (dst_fbo) {
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo);
    }

    functions_->glUseProgram(program);

    // Bind textures
    for (int i = 0; i < tex_count; i++) {
        functions_->glActiveTexture(GL_TEXTURE0 + i);
        functions_->glBindTexture(GL_TEXTURE_2D, textures[i]);
        PrepareInputTexture(GL_TEXTURE_2D, OAK_FILTER_LINEAR);
    }

    // Parse uniforms_json and set uniform values
    if (uniforms_json && uniforms_json[0]) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray(uniforms_json));
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                const QString name = it.key();
                const QJsonValue val = it.value();
                if (!val.isObject()) continue;
                QJsonValue type_v = val.toObject().value(QStringLiteral("type"));
                if (!type_v.isString()) continue;
                QString type = type_v.toString();
                GLint loc = functions_->glGetUniformLocation(program, name.toUtf8().constData());
                if (loc < 0) continue;
                if (type == QLatin1String("texture")) {
                    int unit = val.toObject().value(QStringLiteral("unit")).toInt(0);
                    functions_->glUniform1i(loc, unit);
                } else if (type == QLatin1String("int")) {
                    int v = val.toObject().value(QStringLiteral("value")).toInt(0);
                    functions_->glUniform1i(loc, v);
                } else if (type == QLatin1String("float")) {
                    double v = val.toObject().value(QStringLiteral("value")).toDouble(0.0);
                    functions_->glUniform1f(loc, static_cast<GLfloat>(v));
                } else if (type == QLatin1String("vec2")) {
                    QJsonArray arr = val.toObject().value(QStringLiteral("value")).toArray();
                    GLfloat fv[2] = { static_cast<GLfloat>(arr.size() > 0 ? arr[0].toDouble() : 0.0),
                                      static_cast<GLfloat>(arr.size() > 1 ? arr[1].toDouble() : 0.0) };
                    functions_->glUniform2fv(loc, 1, fv);
                } else if (type == QLatin1String("vec3")) {
                    QJsonArray arr = val.toObject().value(QStringLiteral("value")).toArray();
                    GLfloat fv[3] = { static_cast<GLfloat>(arr.size() > 0 ? arr[0].toDouble() : 0.0),
                                      static_cast<GLfloat>(arr.size() > 1 ? arr[1].toDouble() : 0.0),
                                      static_cast<GLfloat>(arr.size() > 2 ? arr[2].toDouble() : 0.0) };
                    functions_->glUniform3fv(loc, 1, fv);
                } else if (type == QLatin1String("vec4")) {
                    QJsonArray arr = val.toObject().value(QStringLiteral("value")).toArray();
                    GLfloat fv[4] = { static_cast<GLfloat>(arr.size() > 0 ? arr[0].toDouble() : 0.0),
                                      static_cast<GLfloat>(arr.size() > 1 ? arr[1].toDouble() : 0.0),
                                      static_cast<GLfloat>(arr.size() > 2 ? arr[2].toDouble() : 0.0),
                                      static_cast<GLfloat>(arr.size() > 3 ? arr[3].toDouble() : 0.0) };
                    functions_->glUniform4fv(loc, 1, fv);
                } else if (type == QLatin1String("mat4")) {
                    QJsonArray arr = val.toObject().value(QStringLiteral("value")).toArray();
                    GLfloat fv[16];
                    for (int i = 0; i < 16; ++i) {
                        fv[i] = static_cast<GLfloat>(i < arr.size() ? arr[i].toDouble() : (i % 5 == 0 ? 1.0 : 0.0));
                    }
                    functions_->glUniformMatrix4fv(loc, 1, GL_FALSE, fv);
                }
            }
        }
    }

    // Draw fullscreen quad
    QOpenGLBuffer vert_vbo(QOpenGLBuffer::VertexBuffer);
    vert_vbo.create();
    vert_vbo.bind();
    vert_vbo.allocate(kBlitVertices.constData(),
                      kBlitVertices.size() * sizeof(GLfloat));
    vert_vbo.release();

    QOpenGLBuffer tex_vbo(QOpenGLBuffer::VertexBuffer);
    tex_vbo.create();
    tex_vbo.bind();
    tex_vbo.allocate(kBlitTexcoords.constData(),
                     kBlitTexcoords.size() * sizeof(GLfloat));
    tex_vbo.release();

    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();

    GLint vertex_loc = functions_->glGetAttribLocation(program, "a_position");
    if (vertex_loc != -1) {
        vert_vbo.bind();
        functions_->glEnableVertexAttribArray(vertex_loc);
        functions_->glVertexAttribPointer(vertex_loc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        vert_vbo.release();
    }

    GLint texcoord_loc = functions_->glGetAttribLocation(program, "a_texcoord");
    if (texcoord_loc != -1) {
        tex_vbo.bind();
        functions_->glEnableVertexAttribArray(texcoord_loc);
        functions_->glVertexAttribPointer(texcoord_loc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        tex_vbo.release();
    }

    functions_->glDrawArrays(GL_TRIANGLES, 0, kBlitVertices.size() / 3);

    functions_->glUseProgram(0);
    for (int i = tex_count - 1; i >= 0; i--) {
        functions_->glActiveTexture(GL_TEXTURE0 + i);
        functions_->glBindTexture(GL_TEXTURE_2D, 0);
    }

    vert_vbo.destroy();
    tex_vbo.destroy();
    vao.release();
    vao.destroy();

    if (dst_fbo) {
        GLuint default_fbo = context_ ? context_->defaultFramebufferObject() : 0;
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
    }
}

void OpenGLRenderer::DrawWithShaderToTexture(GLuint program, const char *uniforms_json,
                                              GLuint *textures, int tex_count,
                                              GLuint dest_tex)
{
    if (!functions_ || !framebuffer_) return;
    functions_->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    functions_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, dest_tex, 0);
    DrawWithShader(program, uniforms_json, textures, tex_count, 0);
    GLuint default_fbo = context_ ? context_->defaultFramebufferObject() : 0;
    functions_->glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
}

void OpenGLRenderer::DrawWithShaderEx(GLuint program,
                                      const OakShaderUniform *uniforms, int uniform_count,
                                      GLuint *textures, int tex_count, GLuint dst_fbo)
{
    if (!program) return;

    if (dst_fbo) {
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo);
    }

    functions_->glUseProgram(program);

    // Bind textures
    for (int i = 0; i < tex_count; i++) {
        functions_->glActiveTexture(GL_TEXTURE0 + i);
        functions_->glBindTexture(GL_TEXTURE_2D, textures[i]);
        PrepareInputTexture(GL_TEXTURE_2D, OAK_FILTER_LINEAR);
    }

    // Apply binary uniforms
    for (int i = 0; i < uniform_count; i++) {
        const OakShaderUniform &u = uniforms[i];
        GLint loc = functions_->glGetUniformLocation(program, u.name);
        if (loc < 0) continue;
        switch (u.type) {
        case OAK_UNIFORM_TEXTURE:
            functions_->glUniform1i(loc, u.texture_unit);
            break;
        case OAK_UNIFORM_INT:
            functions_->glUniform1i(loc, u.value.i);
            break;
        case OAK_UNIFORM_FLOAT:
            functions_->glUniform1f(loc, u.value.f);
            break;
        case OAK_UNIFORM_VEC2:
            functions_->glUniform2fv(loc, 1, u.value.vec2);
            break;
        case OAK_UNIFORM_VEC3:
            functions_->glUniform3fv(loc, 1, u.value.vec3);
            break;
        case OAK_UNIFORM_VEC4:
            functions_->glUniform4fv(loc, 1, u.value.vec4);
            break;
        case OAK_UNIFORM_MAT4:
            functions_->glUniformMatrix4fv(loc, 1, GL_FALSE, u.value.mat4);
            break;
        }
    }

    // Draw fullscreen quad
    QOpenGLBuffer vert_vbo(QOpenGLBuffer::VertexBuffer);
    vert_vbo.create();
    vert_vbo.bind();
    vert_vbo.allocate(kBlitVertices.constData(),
                      kBlitVertices.size() * sizeof(GLfloat));
    vert_vbo.release();

    QOpenGLBuffer tex_vbo(QOpenGLBuffer::VertexBuffer);
    tex_vbo.create();
    tex_vbo.bind();
    tex_vbo.allocate(kBlitTexcoords.constData(),
                     kBlitTexcoords.size() * sizeof(GLfloat));
    tex_vbo.release();

    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();

    GLint vertex_loc = functions_->glGetAttribLocation(program, "a_position");
    if (vertex_loc != -1) {
        vert_vbo.bind();
        functions_->glEnableVertexAttribArray(vertex_loc);
        functions_->glVertexAttribPointer(vertex_loc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        vert_vbo.release();
    }

    GLint texcoord_loc = functions_->glGetAttribLocation(program, "a_texcoord");
    if (texcoord_loc != -1) {
        tex_vbo.bind();
        functions_->glEnableVertexAttribArray(texcoord_loc);
        functions_->glVertexAttribPointer(texcoord_loc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        tex_vbo.release();
    }

    functions_->glDrawArrays(GL_TRIANGLES, 0, kBlitVertices.size() / 3);

    functions_->glUseProgram(0);
    for (int i = tex_count - 1; i >= 0; i--) {
        functions_->glActiveTexture(GL_TEXTURE0 + i);
        functions_->glBindTexture(GL_TEXTURE_2D, 0);
    }

    vert_vbo.destroy();
    tex_vbo.destroy();
    vao.release();
    vao.destroy();

    if (dst_fbo) {
        GLuint default_fbo = context_ ? context_->defaultFramebufferObject() : 0;
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
    }
}

void OpenGLRenderer::DrawWithShaderToTextureEx(GLuint program,
                                               const OakShaderUniform *uniforms, int uniform_count,
                                               GLuint *textures, int tex_count,
                                               GLuint dest_tex)
{
    if (!functions_ || !framebuffer_) return;
    functions_->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    functions_->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, dest_tex, 0);
    DrawWithShaderEx(program, uniforms, uniform_count, textures, tex_count, 0);
    GLuint default_fbo = context_ ? context_->defaultFramebufferObject() : 0;
    functions_->glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
}

void OpenGLRenderer::BlitYUVToRGBA(GLuint y_tex, GLuint u_tex, GLuint v_tex,
                                    GLuint dst_fbo, int width, int height,
                                    const float *color_matrix_3x3, bool full_range)
{
    if (!functions_) return;
    EnsureContextCurrent("BlitYUVToRGBA");

    GLuint program = CompileShader(kYUVBlitVert, kYUVBlitFrag);
    if (!program) {
        qWarning() << "Failed to compile YUV blit shader";
        return;
    }

    if (dst_fbo) {
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo);
    }
    functions_->glViewport(0, 0, width, height);

    functions_->glUseProgram(program);

    // Bind Y/U/V textures
    functions_->glActiveTexture(GL_TEXTURE0);
    functions_->glBindTexture(GL_TEXTURE_2D, y_tex);
    PrepareInputTexture(GL_TEXTURE_2D, OAK_FILTER_LINEAR);
    GLint y_loc = functions_->glGetUniformLocation(program, "u_yTex");
    if (y_loc != -1) functions_->glUniform1i(y_loc, 0);

    functions_->glActiveTexture(GL_TEXTURE1);
    functions_->glBindTexture(GL_TEXTURE_2D, u_tex);
    PrepareInputTexture(GL_TEXTURE_2D, OAK_FILTER_LINEAR);
    GLint u_loc = functions_->glGetUniformLocation(program, "u_uTex");
    if (u_loc != -1) functions_->glUniform1i(u_loc, 1);

    functions_->glActiveTexture(GL_TEXTURE2);
    functions_->glBindTexture(GL_TEXTURE_2D, v_tex);
    PrepareInputTexture(GL_TEXTURE_2D, OAK_FILTER_LINEAR);
    GLint v_loc = functions_->glGetUniformLocation(program, "u_vTex");
    if (v_loc != -1) functions_->glUniform1i(v_loc, 2);

    // Set color matrix (3x3, row-major)
    GLint mat_loc = functions_->glGetUniformLocation(program, "u_colorMatrix");
    if (mat_loc != -1 && color_matrix_3x3) {
        functions_->glUniformMatrix3fv(mat_loc, 1, GL_FALSE, color_matrix_3x3);
    }

    GLint range_loc = functions_->glGetUniformLocation(program, "u_fullRange");
    if (range_loc != -1) {
        functions_->glUniform1i(range_loc, full_range ? 1 : 0);
    }

    // Draw fullscreen quad
    QOpenGLBuffer vert_vbo(QOpenGLBuffer::VertexBuffer);
    vert_vbo.create();
    vert_vbo.bind();
    vert_vbo.allocate(kBlitVertices.constData(),
                      kBlitVertices.size() * sizeof(GLfloat));
    vert_vbo.release();

    QOpenGLBuffer tex_vbo(QOpenGLBuffer::VertexBuffer);
    tex_vbo.create();
    tex_vbo.bind();
    tex_vbo.allocate(kBlitTexcoords.constData(),
                     kBlitTexcoords.size() * sizeof(GLfloat));
    tex_vbo.release();

    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();

    GLint pos_loc = functions_->glGetAttribLocation(program, "a_position");
    if (pos_loc != -1) {
        vert_vbo.bind();
        functions_->glEnableVertexAttribArray(pos_loc);
        functions_->glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        vert_vbo.release();
    }

    GLint tc_loc = functions_->glGetAttribLocation(program, "a_texcoord");
    if (tc_loc != -1) {
        tex_vbo.bind();
        functions_->glEnableVertexAttribArray(tc_loc);
        functions_->glVertexAttribPointer(tc_loc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        tex_vbo.release();
    }

    functions_->glDrawArrays(GL_TRIANGLES, 0, kBlitVertices.size() / 3);

    // Cleanup
    functions_->glUseProgram(0);
    for (int i = 2; i >= 0; i--) {
        functions_->glActiveTexture(GL_TEXTURE0 + i);
        functions_->glBindTexture(GL_TEXTURE_2D, 0);
    }

    vert_vbo.destroy();
    tex_vbo.destroy();
    vao.release();
    vao.destroy();

    DestroyShader(program);

    if (dst_fbo) {
        GLuint default_fbo = context_ ? context_->defaultFramebufferObject() : 0;
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
    }
}

/* ================================================================ */
/*  Readback                                                         */
/* ================================================================ */

bool OpenGLRenderer::Readback(GLuint fbo, int width, int height,
                              OakRenderPixelFormat fmt,
                              void **out_data, int *out_stride)
{
    if (!EnsureContextCurrent(__FUNCTION__)) return false;

    if (fbo) {
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    int channels = 4;
    int bytes_per_channel = 0;
    switch (fmt) {
    case OAK_RENDER_PIX_FMT_RGBA8:
    case OAK_RENDER_PIX_FMT_R8:
    case OAK_RENDER_PIX_FMT_RG8:
        bytes_per_channel = 1;
        break;
    case OAK_RENDER_PIX_FMT_RGBA16:
        bytes_per_channel = 2;
        break;
    case OAK_RENDER_PIX_FMT_RGBA32F:
        bytes_per_channel = 4;
        break;
    }

    if (bytes_per_channel == 0) return false;

    int stride = width * channels * bytes_per_channel;
    void *data = malloc(stride * height);
    if (!data) return false;

    functions_->glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    functions_->glFinish();

    GLenum pixel_fmt = GetPixelFormat(channels);
    GLenum pixel_type = GetPixelType(fmt);
    functions_->glReadPixels(0, 0, width, height, pixel_fmt, pixel_type, data);

    if (out_data) *out_data = data;
    if (out_stride) *out_stride = stride;

    if (fbo) {
        GLuint default_fbo = context_ ? context_->defaultFramebufferObject() : 0;
        functions_->glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
    }

    return true;
}

/* ================================================================ */
/*  Font (stub)                                                      */
/* ================================================================ */

GLuint OpenGLRenderer::LoadFont(const char *path, float size)
{
    (void)path;
    (void)size;
    // TODO: implement font atlas using FreeType or Qt font rendering
    return 0;
}

void OpenGLRenderer::DestroyFont(GLuint font)
{
    (void)font;
    // TODO
}

/* ================================================================ */
/*  Pixel query                                                      */
/* ================================================================ */

void OpenGLRenderer::GetPixel(GLuint tex, float x, float y, float *out_rgba)
{
    if (!out_rgba) return;
    out_rgba[0] = out_rgba[1] = out_rgba[2] = out_rgba[3] = 0.0f;

    if (!EnsureContextCurrent(__FUNCTION__)) return;

    AttachTextureAsDestination(tex);

    // Read 1x1 pixel (RGBA float)
    float data[4] = {0};
    functions_->glReadPixels(static_cast<GLint>(x), static_cast<GLint>(y),
                             1, 1, GL_RGBA, GL_FLOAT, data);

    out_rgba[0] = data[0];
    out_rgba[1] = data[1];
    out_rgba[2] = data[2];
    out_rgba[3] = data[3];

    DetachTextureAsDestination();
}

/* ================================================================ */
/*  Flush                                                            */
/* ================================================================ */

void OpenGLRenderer::Flush()
{
    if (!functions_) return;

#if defined(Q_OS_MAC)
    // macOS uses Tile-Based Deferred Rendering (TBDR). glFlush() does not
    // guarantee that tile memory has been written back to texture memory.
    functions_->glFinish();
#else
    functions_->glFlush();
#endif
}

} // namespace oakgl
