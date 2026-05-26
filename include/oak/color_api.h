/*
 *  oakcolor.so C API
 *  Color management: OCIO config management, color space conversion, LUT application,
 *  display transform. Internal use of OpenColorIO; OCIO C++ API is fully hidden.
 *
 *  Core convention: the full-pipeline internal working space is ACEScg
 *  (AP1 primaries, scene-referred linear). All modules use oakcolor.so for
 *  IDT (input device transform) and ODT (output device transform).
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

/**
 * @brief Load an OCIO configuration file.
 * @param config_path Path to the .ocio config file. Pass NULL to use the built-in default.
 * @return Config handle, or NULL on failure.
 */
OakColorConfigHandle oak_color_config_load(const char* config_path);

/**
 * @brief Free a config handle.
 * @param config Config handle (NULL is silently ignored).
 */
void                 oak_color_config_free(OakColorConfigHandle config);

/**
 * @brief Get the number of color spaces in the config.
 * @param config Config handle.
 * @return Number of color spaces, or 0/negative on error/null.
 */
int          oak_color_config_space_count(OakColorConfigHandle config);

/**
 * @brief Get the name of a color space by index.
 * @param config Config handle.
 * @param index  Color space index (0-based).
 * @return Color space name string (constant pointer, do not free).
 */
const char*  oak_color_config_space_name(OakColorConfigHandle config, int index);

/**
 * @brief Look up a color space by name.
 * @param config Config handle.
 * @param name   Color space name, e.g. "ACES - ACEScg".
 * @return Color space handle, or NULL if not found.
 */
OakColorSpaceHandle oak_color_config_get_space(OakColorConfigHandle config, const char* name);

/**
 * @brief Get the default input color space (for un-tagged footage).
 * @param config Config handle.
 * @return Default input space name string.
 */
const char*         oak_color_config_default_input_space(OakColorConfigHandle config);

/**
 * @brief Get the default display device name.
 * @param config Config handle.
 * @return Default display name string.
 */
const char*         oak_color_config_default_display(OakColorConfigHandle config);

/**
 * @brief Get the number of views for a given display.
 * @param config       Config handle.
 * @param display_name Display device name.
 * @return Number of views.
 */
int          oak_color_config_display_view_count(OakColorConfigHandle config, const char* display_name);

/**
 * @brief Get the name of a view for a given display.
 * @param config       Config handle.
 * @param display_name Display device name.
 * @param view_index   View index (0-based).
 * @return View name string.
 */
const char*  oak_color_config_display_view_name(OakColorConfigHandle config, const char* display_name, int view_index);

/* ------------------------------------------------------------------ */
/*  Color processor                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a color transformation processor.
 * @param config         Config handle.
 * @param src_space_name Source color space name.
 * @param dst_space_name Destination color space name.
 * @return Processor handle, or NULL on failure (e.g. color space does not exist).
 * @note Processors can be reused multiple times and are thread-safe.
 *
 * Common transform pairs in the ACEScg pipeline:
 *   - IDT: "Input - Rec.709" -> "ACES - ACEScg"
 *   - IDT: "Input - ARRI - V3 LogC (EI800) - Wide Gamut" -> "ACES - ACEScg"
 *   - ODT: "ACES - ACEScg" -> "Output - Rec.709"
 *   - ODT: "ACES - ACEScg" -> "Output - Rec.2020 PQ"
 *   - Display: "ACES - ACEScg" -> "Output - sRGB" (via View Transform)
 */
OakColorProcessorHandle oak_color_processor_create(OakColorConfigHandle config,
                                                   const char* src_space_name,
                                                   const char* dst_space_name);

/**
 * @brief Create a processor from a LUT file.
 * @param lut_path     LUT file path (.cube, .spi3d, etc.).
 * @param input_space  Color space on the LUT input side.
 * @param output_space Color space on the LUT output side.
 * @return Processor handle, or NULL on failure.
 */
OakColorProcessorHandle oak_color_processor_create_from_lut(const char* lut_path,
                                                            const char* input_space,
                                                            const char* output_space);

/**
 * @brief Free a processor handle.
 * @param processor Processor handle (NULL is silently ignored).
 */
void oak_color_processor_free(OakColorProcessorHandle processor);

/**
 * @brief Apply color transformation to an image buffer (CPU path).
 * @param processor  Processor handle.
 * @param width      Image width.
 * @param height     Image height.
 * @param in_data    Input RGBA float32 data.
 * @param out_data   Output buffer (caller-allocated, same size as input).
 * @param pix_layout Pixel layout: 0 = interleaved RGBA, 1 = planar (RRRR...GGGG...BBBB...AAAA...).
 * @return 0 on success, non-zero on failure.
 * @note This is the CPU implementation of IDT/ODT. The GPU path uses
 *       oak_renderer_draw_with_shader + OCIO GPU LUT.
 */
int  oak_color_processor_apply(OakColorProcessorHandle processor,
                               int width, int height,
                               const float* in_data, float* out_data,
                               int pix_layout);

/**
 * @brief Apply color transformation to a single pixel (fast lookup).
 * @param processor  Processor handle.
 * @param in_rgba    Input RGBA float32[4].
 * @param out_rgba   Output RGBA float32[4] (may alias in_rgba).
 */
void oak_color_processor_apply_pixel(OakColorProcessorHandle processor,
                                     const float* in_rgba, float* out_rgba);

/* ------------------------------------------------------------------ */
/*  Display transform (for viewer preview)                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a display transform for viewer preview.
 * @param config         Config handle.
 * @param input_space    Input image color space. In the ACEScg pipeline this is usually "ACES - ACEScg".
 * @param display_name   Display device name (e.g. "sRGB", "Rec.709", "Rec.2020", "P3").
 * @param view_name      View name (e.g. "ACES 1.0 SDR-video", "ACES 1.0 HDR-video").
 * @param look_name      Optional Look name (NULL for none). E.g. "Neutral", "Warm", "Cool".
 * @param exposure_fstop Exposure adjustment in EV. Used for preview exposure tweak; does not affect actual output.
 * @param display_gamma  Display gamma adjustment.
 * @return Display transform handle, or NULL on failure.
 * @note View Transform = RRT (Reference Render Transform) + ODT (Output Device Transform).
 *       It converts scene-referred linear ACEScg data to display-referred non-linear data.
 *       NEVER display ACEScg data directly without View Transform (it will look gray and overexposed).
 */
OakDisplayTransformHandle oak_display_transform_create(OakColorConfigHandle config,
                                                        const char* input_space,
                                                        const char* display_name,
                                                        const char* view_name,
                                                        const char* look_name,
                                                        float exposure_fstop,
                                                        float display_gamma);

/**
 * @brief Free a display transform handle.
 * @param transform Display transform handle (NULL is silently ignored).
 */
void oak_display_transform_free(OakDisplayTransformHandle transform);

/**
 * @brief Apply a display transform to an image buffer.
 * @param transform  Display transform handle.
 * @param width      Image width.
 * @param height     Image height.
 * @param in_data    Input RGBA float32.
 * @param out_data   Output RGBA float32 (caller-allocated).
 * @param pix_layout Pixel layout (0 = interleaved, 1 = planar).
 * @return 0 on success, non-zero on failure.
 */
int  oak_display_transform_apply(OakDisplayTransformHandle transform,
                                 int width, int height,
                                 const float* in_data, float* out_data,
                                 int pix_layout);

/* ------------------------------------------------------------------ */
/*  High-level processor creation                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a transform processor with explicit direction.
 * @param config    Config handle.
 * @param src_space Source color space.
 * @param dst_space Destination color space.
 * @param direction 0 = forward, 1 = inverse.
 * @return Processor handle, or NULL on failure.
 */
OakColorProcessorHandle oak_color_processor_create_transform(
    OakColorConfigHandle config,
    const char* src_space,
    const char* dst_space,
    int direction);

/**
 * @brief Create a display processor with explicit direction.
 * @param config       Config handle.
 * @param input_space  Input color space.
 * @param display_name Display device name.
 * @param view_name    View name.
 * @param look_name    Look name (can be NULL).
 * @param direction    0 = forward, 1 = inverse.
 * @return Processor handle, or NULL on failure.
 */
OakColorProcessorHandle oak_color_processor_create_display(
    OakColorConfigHandle config,
    const char* input_space,
    const char* display_name,
    const char* view_name,
    const char* look_name,
    int direction);

/* ------------------------------------------------------------------ */
/*  GPU Shader generation                                               */
/* ------------------------------------------------------------------ */

typedef struct OakColorGPUShader* OakColorGPUShaderHandle;

/**
 * @brief Generate a GPU shader from a color processor.
 * @param processor      Processor handle.
 * @param function_name  Shader function name.
 * @param resource_prefix Resource prefix for shader uniforms.
 * @return GPU shader handle, or NULL on failure.
 */
OakColorGPUShaderHandle oak_color_gpu_shader_create(OakColorProcessorHandle processor,
                                                    const char* function_name,
                                                    const char* resource_prefix);

/**
 * @brief Free a GPU shader handle.
 * @param shader Shader handle (NULL is silently ignored).
 */
void oak_color_gpu_shader_free(OakColorGPUShaderHandle shader);

/**
 * @brief Get the shader source text.
 * @param shader GPU shader handle.
 * @return Shader source string (constant pointer, do not free).
 */
const char* oak_color_gpu_shader_get_text(OakColorGPUShaderHandle shader);

/**
 * @brief Get the number of 3D LUTs required by the shader.
 * @param shader GPU shader handle.
 * @return Number of 3D LUTs.
 */
int oak_color_gpu_shader_get_3d_lut_count(OakColorGPUShaderHandle shader);

/**
 * @brief Get 3D LUT metadata.
 * @param shader            GPU shader handle.
 * @param index             LUT index.
 * @param out_name          Output LUT name.
 * @param out_sampler       Output sampler name.
 * @param out_edge_len      Output edge length.
 * @param out_interpolation Output interpolation mode.
 * @param out_values        Output float data pointer.
 * @return 0 on success, non-zero on failure.
 */
int oak_color_gpu_shader_get_3d_lut(OakColorGPUShaderHandle shader, int index,
                                    const char** out_name, const char** out_sampler,
                                    unsigned int* out_edge_len, int* out_interpolation,
                                    const float** out_values);

/**
 * @brief Get the number of textures required by the shader.
 * @param shader GPU shader handle.
 * @return Number of textures.
 */
int oak_color_gpu_shader_get_texture_count(OakColorGPUShaderHandle shader);

/**
 * @brief Get texture metadata.
 * @param shader             GPU shader handle.
 * @param index              Texture index.
 * @param out_name           Output texture name.
 * @param out_sampler        Output sampler name.
 * @param out_width          Output width.
 * @param out_height         Output height.
 * @param out_channel_count  Output channel count.
 * @param out_dimensions     Output dimensions.
 * @param out_interpolation  Output interpolation mode.
 * @param out_values         Output float data pointer.
 * @return 0 on success, non-zero on failure.
 */
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

/**
 * @brief Create a GradingPrimary transform.
 * @param style 0 = linear, 1 = video.
 * @return GradingPrimary handle, or NULL on failure.
 */
OakColorGradingPrimaryHandle oak_color_grading_primary_create(int style);

/**
 * @brief Free a GradingPrimary handle.
 * @param gp GradingPrimary handle (NULL is silently ignored).
 */
void oak_color_grading_primary_free(OakColorGradingPrimaryHandle gp);

/**
 * @brief Set dynamic mode.
 * @param gp      GradingPrimary handle.
 * @param dynamic True for dynamic (animated) parameters.
 */
void oak_color_grading_primary_set_dynamic(OakColorGradingPrimaryHandle gp, bool dynamic);

/**
 * @brief Set transform direction.
 * @param gp        GradingPrimary handle.
 * @param direction 0 = forward, 1 = inverse.
 */
void oak_color_grading_primary_set_direction(OakColorGradingPrimaryHandle gp, int direction);

/**
 * @brief Set contrast (RGB + master).
 * @param gp    GradingPrimary handle.
 * @param rgbm  Float array[4] = {R, G, B, master}.
 */
void oak_color_grading_primary_set_contrast(OakColorGradingPrimaryHandle gp, const float* rgbm);

/**
 * @brief Set offset (RGB + master).
 * @param gp    GradingPrimary handle.
 * @param rgbm  Float array[4] = {R, G, B, master}.
 */
void oak_color_grading_primary_set_offset(OakColorGradingPrimaryHandle gp, const float* rgbm);

/**
 * @brief Set exposure (RGB + master).
 * @param gp    GradingPrimary handle.
 * @param rgbm  Float array[4] = {R, G, B, master}.
 */
void oak_color_grading_primary_set_exposure(OakColorGradingPrimaryHandle gp, const float* rgbm);

/**
 * @brief Set saturation.
 * @param gp  GradingPrimary handle.
 * @param val Saturation value (1.0 = neutral).
 */
void oak_color_grading_primary_set_saturation(OakColorGradingPrimaryHandle gp, float val);

/**
 * @brief Set pivot.
 * @param gp  GradingPrimary handle.
 * @param val Pivot value.
 */
void oak_color_grading_primary_set_pivot(OakColorGradingPrimaryHandle gp, float val);

/**
 * @brief Set black clamp.
 * @param gp  GradingPrimary handle.
 * @param val Clamp value.
 */
void oak_color_grading_primary_set_clamp_black(OakColorGradingPrimaryHandle gp, float val);

/**
 * @brief Set white clamp.
 * @param gp  GradingPrimary handle.
 * @param val Clamp value.
 */
void oak_color_grading_primary_set_clamp_white(OakColorGradingPrimaryHandle gp, float val);

/**
 * @brief Get the "no clamp" sentinel value for black.
 * @return Sentinel value.
 */
float oak_color_grading_primary_no_clamp_black(void);

/**
 * @brief Get the "no clamp" sentinel value for white.
 * @return Sentinel value.
 */
float oak_color_grading_primary_no_clamp_white(void);

/**
 * @brief Create a processor from a GradingPrimary transform.
 * @param config       Config handle.
 * @param input_space  Input color space.
 * @param output_space Output color space.
 * @param gp           GradingPrimary handle.
 * @param direction    0 = forward, 1 = inverse.
 * @return Processor handle, or NULL on failure.
 */
OakColorProcessorHandle oak_color_processor_create_from_grading(
    OakColorConfigHandle config,
    const char* input_space,
    const char* output_space,
    OakColorGradingPrimaryHandle gp,
    int direction);

/* ------------------------------------------------------------------ */
/*  Metadata & reference space                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Get the reference color space name (e.g. "ACES - ACES2065-1").
 * @param config Config handle.
 * @return Reference space name string.
 */
const char* oak_color_config_reference_space_name(OakColorConfigHandle config);

/**
 * @brief Check whether two color spaces are equivalent (no conversion needed).
 * @param a Color space handle A.
 * @param b Color space handle B.
 * @return True if equivalent.
 */
bool        oak_color_space_equal(OakColorSpaceHandle a, OakColorSpaceHandle b);

#ifdef __cplusplus
}
#endif

#endif /* OAK_COLOR_API_H */
