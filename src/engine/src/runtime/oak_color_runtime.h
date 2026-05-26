/***  Oak Video Editor - OakColor Runtime Loader  Copyright (C) 2025 mikesolar  ***/

#ifndef OAK_COLOR_RUNTIME_H
#define OAK_COLOR_RUNTIME_H

#include "runtime_loader.h"
#include "oak/color_api.h"

namespace olive {

class OakColorRuntime : public OakRuntimeLoader {
public:
    static OakColorRuntime* Instance();

    bool Load();

    /* --- config --- */
    OakColorConfigHandle (*config_load)(const char* path) = nullptr;
    void (*config_free)(OakColorConfigHandle config) = nullptr;
    int (*config_space_count)(OakColorConfigHandle config) = nullptr;
    const char* (*config_space_name)(OakColorConfigHandle config, int index) = nullptr;
    const char* (*config_default_input_space)(OakColorConfigHandle config) = nullptr;
    const char* (*config_default_display)(OakColorConfigHandle config) = nullptr;
    int (*config_display_view_count)(OakColorConfigHandle config, const char* display) = nullptr;
    const char* (*config_display_view_name)(OakColorConfigHandle config, const char* display, int index) = nullptr;
    const char* (*config_reference_space_name)(OakColorConfigHandle config) = nullptr;

    /* --- processor --- */
    OakColorProcessorHandle (*processor_create_transform)(OakColorConfigHandle config,
                                                          const char* src,
                                                          const char* dst,
                                                          int direction) = nullptr;
    OakColorProcessorHandle (*processor_create_display)(OakColorConfigHandle config,
                                                        const char* input_space,
                                                        const char* display,
                                                        const char* view,
                                                        const char* look,
                                                        int direction) = nullptr;
    OakColorProcessorHandle (*processor_create_from_grading)(OakColorConfigHandle config,
                                                             const char* input_space,
                                                             const char* output_space,
                                                             OakColorGradingPrimaryHandle gp,
                                                             int direction) = nullptr;
    void (*processor_free)(OakColorProcessorHandle processor) = nullptr;
    int (*processor_apply)(OakColorProcessorHandle processor,
                           int width, int height,
                           const float* in_data, float* out_data,
                           int pix_layout) = nullptr;
    void (*processor_apply_pixel)(OakColorProcessorHandle processor,
                                  const float* in_rgba, float* out_rgba) = nullptr;

    /* --- gpu shader --- */
    OakColorGPUShaderHandle (*gpu_shader_create)(OakColorProcessorHandle processor,
                                                 const char* function_name,
                                                 const char* resource_prefix) = nullptr;
    void (*gpu_shader_free)(OakColorGPUShaderHandle shader) = nullptr;
    const char* (*gpu_shader_get_text)(OakColorGPUShaderHandle shader) = nullptr;
    int (*gpu_shader_get_3d_lut_count)(OakColorGPUShaderHandle shader) = nullptr;
    int (*gpu_shader_get_3d_lut)(OakColorGPUShaderHandle shader, int index,
                                 const char** out_name, const char** out_sampler,
                                 unsigned int* out_edge_len, int* out_interpolation,
                                 const float** out_values) = nullptr;
    int (*gpu_shader_get_texture_count)(OakColorGPUShaderHandle shader) = nullptr;
    int (*gpu_shader_get_texture)(OakColorGPUShaderHandle shader, int index,
                                  const char** out_name, const char** out_sampler,
                                  unsigned int* out_width, unsigned int* out_height,
                                  int* out_channel_count, int* out_dimensions,
                                  int* out_interpolation,
                                  const float** out_values) = nullptr;

    /* --- grading primary --- */
    OakColorGradingPrimaryHandle (*grading_primary_create)(int style) = nullptr;
    void (*grading_primary_free)(OakColorGradingPrimaryHandle gp) = nullptr;
    void (*grading_primary_set_dynamic)(OakColorGradingPrimaryHandle gp, bool dynamic) = nullptr;
    void (*grading_primary_set_direction)(OakColorGradingPrimaryHandle gp, int direction) = nullptr;
    void (*grading_primary_set_contrast)(OakColorGradingPrimaryHandle gp, const float* rgbm) = nullptr;
    void (*grading_primary_set_offset)(OakColorGradingPrimaryHandle gp, const float* rgbm) = nullptr;
    void (*grading_primary_set_exposure)(OakColorGradingPrimaryHandle gp, const float* rgbm) = nullptr;
    void (*grading_primary_set_saturation)(OakColorGradingPrimaryHandle gp, float val) = nullptr;
    void (*grading_primary_set_pivot)(OakColorGradingPrimaryHandle gp, float val) = nullptr;
    void (*grading_primary_set_clamp_black)(OakColorGradingPrimaryHandle gp, float val) = nullptr;
    void (*grading_primary_set_clamp_white)(OakColorGradingPrimaryHandle gp, float val) = nullptr;
    float (*grading_primary_no_clamp_black)(void) = nullptr;
    float (*grading_primary_no_clamp_white)(void) = nullptr;

private:
    OakColorRuntime() = default;
};

} // namespace olive

#endif // OAK_COLOR_RUNTIME_H
