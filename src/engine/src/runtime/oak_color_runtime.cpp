/***  Oak Video Editor - OakColor Runtime Loader  Copyright (C) 2025 mikesolar  ***/

#include "oak_color_runtime.h"
#include <QDebug>

namespace olive {

OakColorRuntime* OakColorRuntime::Instance()
{
    static OakColorRuntime instance;
    return &instance;
}

bool OakColorRuntime::Load()
{
    if (IsLoaded()) {
        return true;
    }

#if defined(__APPLE__)
    if (!OakRuntimeLoader::Load(QStringLiteral("liboakcolor.dylib"))) {
        return false;
    }
#elif defined(__linux__)
    if (!OakRuntimeLoader::Load(QStringLiteral("liboakcolor.so"))) {
        return false;
    }
#elif defined(_WIN32)
    if (!OakRuntimeLoader::Load(QStringLiteral("liboakcolor.dll"))) {
        return false;
    }
#endif

    config_load = GetSymbol<decltype(config_load)>("oak_color_config_load");
    config_free = GetSymbol<decltype(config_free)>("oak_color_config_free");
    config_space_count = GetSymbol<decltype(config_space_count)>("oak_color_config_space_count");
    config_space_name = GetSymbol<decltype(config_space_name)>("oak_color_config_space_name");
    config_default_input_space = GetSymbol<decltype(config_default_input_space)>("oak_color_config_default_input_space");
    config_default_display = GetSymbol<decltype(config_default_display)>("oak_color_config_default_display");
    config_display_view_count = GetSymbol<decltype(config_display_view_count)>("oak_color_config_display_view_count");
    config_display_view_name = GetSymbol<decltype(config_display_view_name)>("oak_color_config_display_view_name");
    config_reference_space_name = GetSymbol<decltype(config_reference_space_name)>("oak_color_config_reference_space_name");

    processor_create_transform = GetSymbol<decltype(processor_create_transform)>("oak_color_processor_create_transform");
    processor_create_display = GetSymbol<decltype(processor_create_display)>("oak_color_processor_create_display");
    processor_create_from_grading = GetSymbol<decltype(processor_create_from_grading)>("oak_color_processor_create_from_grading");
    processor_free = GetSymbol<decltype(processor_free)>("oak_color_processor_free");
    processor_apply = GetSymbol<decltype(processor_apply)>("oak_color_processor_apply");
    processor_apply_pixel = GetSymbol<decltype(processor_apply_pixel)>("oak_color_processor_apply_pixel");

    gpu_shader_create = GetSymbol<decltype(gpu_shader_create)>("oak_color_gpu_shader_create");
    gpu_shader_free = GetSymbol<decltype(gpu_shader_free)>("oak_color_gpu_shader_free");
    gpu_shader_get_text = GetSymbol<decltype(gpu_shader_get_text)>("oak_color_gpu_shader_get_text");
    gpu_shader_get_3d_lut_count = GetSymbol<decltype(gpu_shader_get_3d_lut_count)>("oak_color_gpu_shader_get_3d_lut_count");
    gpu_shader_get_3d_lut = GetSymbol<decltype(gpu_shader_get_3d_lut)>("oak_color_gpu_shader_get_3d_lut");
    gpu_shader_get_texture_count = GetSymbol<decltype(gpu_shader_get_texture_count)>("oak_color_gpu_shader_get_texture_count");
    gpu_shader_get_texture = GetSymbol<decltype(gpu_shader_get_texture)>("oak_color_gpu_shader_get_texture");

    grading_primary_create = GetSymbol<decltype(grading_primary_create)>("oak_color_grading_primary_create");
    grading_primary_free = GetSymbol<decltype(grading_primary_free)>("oak_color_grading_primary_free");
    grading_primary_set_dynamic = GetSymbol<decltype(grading_primary_set_dynamic)>("oak_color_grading_primary_set_dynamic");
    grading_primary_set_direction = GetSymbol<decltype(grading_primary_set_direction)>("oak_color_grading_primary_set_direction");
    grading_primary_set_contrast = GetSymbol<decltype(grading_primary_set_contrast)>("oak_color_grading_primary_set_contrast");
    grading_primary_set_offset = GetSymbol<decltype(grading_primary_set_offset)>("oak_color_grading_primary_set_offset");
    grading_primary_set_exposure = GetSymbol<decltype(grading_primary_set_exposure)>("oak_color_grading_primary_set_exposure");
    grading_primary_set_saturation = GetSymbol<decltype(grading_primary_set_saturation)>("oak_color_grading_primary_set_saturation");
    grading_primary_set_pivot = GetSymbol<decltype(grading_primary_set_pivot)>("oak_color_grading_primary_set_pivot");
    grading_primary_set_clamp_black = GetSymbol<decltype(grading_primary_set_clamp_black)>("oak_color_grading_primary_set_clamp_black");
    grading_primary_set_clamp_white = GetSymbol<decltype(grading_primary_set_clamp_white)>("oak_color_grading_primary_set_clamp_white");
    grading_primary_no_clamp_black = GetSymbol<decltype(grading_primary_no_clamp_black)>("oak_color_grading_primary_no_clamp_black");
    grading_primary_no_clamp_white = GetSymbol<decltype(grading_primary_no_clamp_white)>("oak_color_grading_primary_no_clamp_white");

    if (!config_load || !processor_create_transform || !processor_apply) {
        qWarning() << "Failed to resolve essential color symbols from oakcolor.so";
        return false;
    }

    return true;
}

} // namespace olive
