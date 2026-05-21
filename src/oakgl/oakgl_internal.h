#ifndef OAKGL_INTERNAL_H
#define OAKGL_INTERNAL_H

#include "oak/renderer_api.h"

#include <string>
#include <vector>

/* Internal C++ structures backing the C API opaque handles.
 * Will wrap existing OpenGL renderer / Qt GL context. */

struct OakRenderer {
    std::string backend_name;
    // TODO: GL context, command queue, resource manager
};

struct OakTexture {
    int width = 0;
    int height = 0;
    OakRenderPixelFormat pix_fmt = OAK_RENDER_PIX_FMT_RGBA8;
    // TODO: native GPU texture handle (GLuint, VkImage, etc.)
};

struct OakTarget {
    int width = 0;
    int height = 0;
    OakRenderPixelFormat pix_fmt = OAK_RENDER_PIX_FMT_RGBA8;
    bool has_depth = false;
    // TODO: FBO / render pass handle
};

struct OakShader {
    std::string name;
    // TODO: shader program handle
};

struct OakFontAtlas {
    std::string font_path;
    float font_size = 12.0f;
    // TODO: glyph cache texture
};

#endif /* OAKGL_INTERNAL_H */
