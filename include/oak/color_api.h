/*
 *  oakcolor.so C API
 *  色彩管理：OCIO 配置管理、色彩空间转换、LUT 应用、显示变换。
 */

#ifndef OAK_COLOR_API_H
#define OAK_COLOR_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Opaque handles                                                      */
/* ------------------------------------------------------------------ */

typedef struct OakColorConfig*    OakColorConfigHandle;
typedef struct OakColorProcessor* OakColorProcessorHandle;
typedef struct OakColorSpace*     OakColorSpaceHandle;
typedef struct OakDisplayTransform* OakDisplayTransformHandle;

/* ------------------------------------------------------------------ */
/*  Config management                                                   */
/* ------------------------------------------------------------------ */

OakColorConfigHandle oak_color_config_load(const char* config_path);
void                 oak_color_config_free(OakColorConfigHandle config);

int          oak_color_config_space_count(OakColorConfigHandle config);
const char*  oak_color_config_space_name(OakColorConfigHandle config, int index);

OakColorSpaceHandle oak_color_config_get_space(OakColorConfigHandle config, const char* name);
const char*         oak_color_config_default_input_space(OakColorConfigHandle config);
const char*         oak_color_config_default_display(OakColorConfigHandle config);

int          oak_color_config_display_view_count(OakColorConfigHandle config, const char* display_name);
const char*  oak_color_config_display_view_name(OakColorConfigHandle config, const char* display_name, int view_index);

/* ------------------------------------------------------------------ */
/*  Color processor                                                     */
/* ------------------------------------------------------------------ */

OakColorProcessorHandle oak_color_processor_create(OakColorConfigHandle config,
                                                   const char* src_space_name,
                                                   const char* dst_space_name);

OakColorProcessorHandle oak_color_processor_create_from_lut(const char* lut_path,
                                                            const char* input_space,
                                                            const char* output_space);

void oak_color_processor_free(OakColorProcessorHandle processor);

int  oak_color_processor_apply(OakColorProcessorHandle processor,
                               int width, int height,
                               const float* in_data, float* out_data,
                               int pix_layout);

void oak_color_processor_apply_pixel(OakColorProcessorHandle processor,
                                     const float* in_rgba, float* out_rgba);

/* ------------------------------------------------------------------ */
/*  Display transform (for viewer preview)                              */
/* ------------------------------------------------------------------ */

OakDisplayTransformHandle oak_display_transform_create(OakColorConfigHandle config,
                                                        const char* input_space,
                                                        const char* display_name,
                                                        const char* view_name,
                                                        const char* look_name,
                                                        float exposure_fstop,
                                                        float display_gamma);

void oak_display_transform_free(OakDisplayTransformHandle transform);

int  oak_display_transform_apply(OakDisplayTransformHandle transform,
                                 int width, int height,
                                 const float* in_data, float* out_data,
                                 int pix_layout);

/* ------------------------------------------------------------------ */
/*  High-level processor creation (replaces direct config->getProcessor) */
/* ------------------------------------------------------------------ */

OakColorProcessorHandle oak_color_processor_create_transform(
    OakColorConfigHandle config,
    const char* src_space,
    const char* dst_space,
    int direction); /* 0 = forward, 1 = inverse */

OakColorProcessorHandle oak_color_processor_create_display(
    OakColorConfigHandle config,
    const char* input_space,
    const char* display_name,
    const char* view_name,
    const char* look_name, /* can be NULL */
    int direction);        /* 0 = forward, 1 = inverse */

/* ------------------------------------------------------------------ */
/*  GPU Shader generation                                               */
/* ------------------------------------------------------------------ */

typedef struct OakColorGPUShader* OakColorGPUShaderHandle;

OakColorGPUShaderHandle oak_color_gpu_shader_create(OakColorProcessorHandle processor,
                                                    const char* function_name,
                                                    const char* resource_prefix);
void oak_color_gpu_shader_free(OakColorGPUShaderHandle shader);
const char* oak_color_gpu_shader_get_text(OakColorGPUShaderHandle shader);

int oak_color_gpu_shader_get_3d_lut_count(OakColorGPUShaderHandle shader);
int oak_color_gpu_shader_get_3d_lut(OakColorGPUShaderHandle shader, int index,
                                    const char** out_name, const char** out_sampler,
                                    unsigned int* out_edge_len, int* out_interpolation,
                                    const float** out_values);

int oak_color_gpu_shader_get_texture_count(OakColorGPUShaderHandle shader);
int oak_color_gpu_shader_get_texture(OakColorGPUShaderHandle shader, int index,
                                     const char** out_name, const char** out_sampler,
                                     unsigned int* out_width, unsigned int* out_height,
                                     int* out_channel_count, int* out_dimensions,
                                     int* out_interpolation,
                                     const float** out_values);

/* ------------------------------------------------------------------ */
/*  GradingPrimary transform                                            */
/* ------------------------------------------------------------------ */

typedef struct OakColorGradingPrimary* OakColorGradingPrimaryHandle;

OakColorGradingPrimaryHandle oak_color_grading_primary_create(int style); /* 0 = lin, 1 = video */
void oak_color_grading_primary_free(OakColorGradingPrimaryHandle gp);

void oak_color_grading_primary_set_dynamic(OakColorGradingPrimaryHandle gp, bool dynamic);
void oak_color_grading_primary_set_direction(OakColorGradingPrimaryHandle gp, int direction); /* 0 = forward, 1 = inverse */

void oak_color_grading_primary_set_contrast(OakColorGradingPrimaryHandle gp, const float* rgbm);
void oak_color_grading_primary_set_offset(OakColorGradingPrimaryHandle gp, const float* rgbm);
void oak_color_grading_primary_set_exposure(OakColorGradingPrimaryHandle gp, const float* rgbm);
void oak_color_grading_primary_set_saturation(OakColorGradingPrimaryHandle gp, float val);
void oak_color_grading_primary_set_pivot(OakColorGradingPrimaryHandle gp, float val);
void oak_color_grading_primary_set_clamp_black(OakColorGradingPrimaryHandle gp, float val);
void oak_color_grading_primary_set_clamp_white(OakColorGradingPrimaryHandle gp, float val);

float oak_color_grading_primary_no_clamp_black(void);
float oak_color_grading_primary_no_clamp_white(void);

OakColorProcessorHandle oak_color_processor_create_from_grading(
    OakColorConfigHandle config,
    const char* input_space,
    const char* output_space,
    OakColorGradingPrimaryHandle gp,
    int direction); /* 0 = forward, 1 = inverse */

/* ------------------------------------------------------------------ */
/*  Metadata & reference space                                          */
/* ------------------------------------------------------------------ */

const char* oak_color_config_reference_space_name(OakColorConfigHandle config);
bool        oak_color_space_equal(OakColorSpaceHandle a, OakColorSpaceHandle b);

#ifdef __cplusplus
}
#endif

#endif /* OAK_COLOR_API_H */
