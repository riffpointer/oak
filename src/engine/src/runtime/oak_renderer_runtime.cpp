/***  Oak Video Editor - Renderer Runtime Loader  Copyright (C) 2025 mikesolar  ***/

#include "oak_renderer_runtime.h"
#include <QDebug>

namespace olive {

OakRendererRuntime* OakRendererRuntime::Instance()
{
    static OakRendererRuntime instance;
    return &instance;
}

bool OakRendererRuntime::Load()
{
    if (IsLoaded()) {
        return true;
    }

#if defined(__APPLE__)
    if (!OakRuntimeLoader::Load(QStringLiteral("liboakgl.dylib"))) {
        return false;
    }
#elif defined(__linux__)
    if (!OakRuntimeLoader::Load(QStringLiteral("liboakgl.so"))) {
        return false;
    }
#elif defined(_WIN32)
    if (!OakRuntimeLoader::Load(QStringLiteral("oakgl.dll"))) {
        return false;
    }
#endif

    renderer_create = GetSymbol<decltype(renderer_create)>("oak_renderer_create");
    renderer_destroy = GetSymbol<decltype(renderer_destroy)>("oak_renderer_destroy");
    renderer_backend_name = GetSymbol<decltype(renderer_backend_name)>("oak_renderer_backend_name");
    renderer_capability = GetSymbol<decltype(renderer_capability)>("oak_renderer_capability");

    texture_upload = GetSymbol<decltype(texture_upload)>("oak_texture_upload");
    texture_upload_from_frame = GetSymbol<decltype(texture_upload_from_frame)>("oak_texture_upload_from_frame");
    texture_create_planar = GetSymbol<decltype(texture_create_planar)>("oak_texture_create_planar");
    texture_wrap_external = GetSymbol<decltype(texture_wrap_external)>("oak_texture_wrap_external");
    texture_destroy = GetSymbol<decltype(texture_destroy)>("oak_texture_destroy");
    texture_size = GetSymbol<decltype(texture_size)>("oak_texture_size");

    target_create = GetSymbol<decltype(target_create)>("oak_target_create");
    target_destroy = GetSymbol<decltype(target_destroy)>("oak_target_destroy");
    target_resize = GetSymbol<decltype(target_resize)>("oak_target_resize");
    target_size = GetSymbol<decltype(target_size)>("oak_target_size");
    target_color_texture = GetSymbol<decltype(target_color_texture)>("oak_target_color_texture");
    target_detach_color_texture = GetSymbol<decltype(target_detach_color_texture)>("oak_target_detach_color_texture");

    renderer_begin = GetSymbol<decltype(renderer_begin)>("oak_renderer_begin");
    renderer_end = GetSymbol<decltype(renderer_end)>("oak_renderer_end");
    renderer_flush = GetSymbol<decltype(renderer_flush)>("oak_renderer_flush");
    renderer_clear_texture = GetSymbol<decltype(renderer_clear_texture)>("oak_renderer_clear_texture");
    renderer_draw_quad = GetSymbol<decltype(renderer_draw_quad)>("oak_renderer_draw_quad");
    renderer_draw_text = GetSymbol<decltype(renderer_draw_text)>("oak_renderer_draw_text");
    renderer_draw_lines = GetSymbol<decltype(renderer_draw_lines)>("oak_renderer_draw_lines");
    renderer_draw_polygon = GetSymbol<decltype(renderer_draw_polygon)>("oak_renderer_draw_polygon");
    renderer_apply_effect = GetSymbol<decltype(renderer_apply_effect)>("oak_renderer_apply_effect");
    renderer_apply_display_transform = GetSymbol<decltype(renderer_apply_display_transform)>("oak_renderer_apply_display_transform");
    renderer_blit_yuv_to_rgba = GetSymbol<decltype(renderer_blit_yuv_to_rgba)>("oak_renderer_blit_yuv_to_rgba");

    renderer_readback = GetSymbol<decltype(renderer_readback)>("oak_renderer_readback");
    renderer_free_readback = GetSymbol<decltype(renderer_free_readback)>("oak_renderer_free_readback");
    renderer_readback_frame = GetSymbol<decltype(renderer_readback_frame)>("oak_renderer_readback_frame");
    texture_download = GetSymbol<decltype(texture_download)>("oak_texture_download");
    renderer_get_pixel = GetSymbol<decltype(renderer_get_pixel)>("oak_renderer_get_pixel");

    shader_compile = GetSymbol<decltype(shader_compile)>("oak_shader_compile");
    shader_destroy = GetSymbol<decltype(shader_destroy)>("oak_shader_destroy");
    renderer_draw_with_shader = GetSymbol<decltype(renderer_draw_with_shader)>("oak_renderer_draw_with_shader");
    renderer_draw_with_shader_ex = GetSymbol<decltype(renderer_draw_with_shader_ex)>("oak_renderer_draw_with_shader_ex");
    renderer_draw_with_shader_to_texture_ex = GetSymbol<decltype(renderer_draw_with_shader_to_texture_ex)>("oak_renderer_draw_with_shader_to_texture_ex");

    font_load = GetSymbol<decltype(font_load)>("oak_font_load");
    font_destroy = GetSymbol<decltype(font_destroy)>("oak_font_destroy");

    if (!renderer_create || !renderer_destroy) {
        qWarning() << "Failed to resolve essential renderer symbols from oakgl.so";
        return false;
    }

    return true;
}

} // namespace olive
