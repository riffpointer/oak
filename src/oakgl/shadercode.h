/***

  Oak Video Editor - Shader code container
  Copyright (C) 2025 mikesolar

***/

#ifndef OAKGL_SHADERCODE_H
#define OAKGL_SHADERCODE_H

#include <QString>

namespace oakgl
{

class ShaderCode {
public:
    ShaderCode(const QString &frag_code = QString(),
               const QString &vert_code = QString())
        : frag_code_(frag_code)
        , vert_code_(vert_code)
    {
    }

    const QString &frag_code() const { return frag_code_; }
    void set_frag_code(const QString &f) { frag_code_ = f; }

    const QString &vert_code() const { return vert_code_; }
    void set_vert_code(const QString &v) { vert_code_ = v; }

private:
    QString frag_code_;
    QString vert_code_;
};

} // namespace oakgl

#endif // OAKGL_SHADERCODE_H
