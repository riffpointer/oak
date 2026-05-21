#include "oak/color_api.h"
#include "oakcolor_internal.h"

#include <cstdlib>
#include <cstring>

/* ------------------------------------------------------------------ */
/*  Config management                                                   */
/* ------------------------------------------------------------------ */

OakColorConfigHandle oak_color_config_load(const char* config_path) {
    (void)config_path;
    // TODO: call OCIO::Config::CreateFromFile() or use built-in default
    return nullptr;
}

void oak_color_config_free(OakColorConfigHandle config) {
    delete config;
}

int oak_color_config_space_count(OakColorConfigHandle config) {
    (void)config;
    return 0; /* TODO */
}

const char* oak_color_config_space_name(OakColorConfigHandle config, int index) {
    (void)config; (void)index;
    return nullptr; /* TODO */
}

OakColorSpaceHandle oak_color_config_get_space(OakColorConfigHandle config, const char* name) {
    (void)config; (void)name;
    return nullptr; /* TODO */
}

const char* oak_color_config_default_input_space(OakColorConfigHandle config) {
    (void)config;
    return nullptr; /* TODO */
}

const char* oak_color_config_default_display(OakColorConfigHandle config) {
    (void)config;
    return nullptr; /* TODO */
}

int oak_color_config_display_view_count(OakColorConfigHandle config, const char* display_name) {
    (void)config; (void)display_name;
    return 0; /* TODO */
}

const char* oak_color_config_display_view_name(OakColorConfigHandle config,
                                               const char* display_name, int view_index) {
    (void)config; (void)display_name; (void)view_index;
    return nullptr; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Color processor                                                     */
/* ------------------------------------------------------------------ */

OakColorProcessorHandle oak_color_processor_create(OakColorConfigHandle config,
                                                   const char* src_space_name,
                                                   const char* dst_space_name) {
    (void)config; (void)src_space_name; (void)dst_space_name;
    return nullptr; /* TODO */
}

OakColorProcessorHandle oak_color_processor_create_from_lut(const char* lut_path,
                                                            const char* input_space,
                                                            const char* output_space) {
    (void)lut_path; (void)input_space; (void)output_space;
    return nullptr; /* TODO */
}

void oak_color_processor_free(OakColorProcessorHandle processor) {
    delete processor;
}

int oak_color_processor_apply(OakColorProcessorHandle processor,
                              int width, int height,
                              const float* in_data, float* out_data,
                              int pix_layout) {
    (void)processor; (void)width; (void)height;
    (void)in_data; (void)out_data; (void)pix_layout;
    return -1; /* TODO */
}

void oak_color_processor_apply_pixel(OakColorProcessorHandle processor,
                                     const float* in_rgba, float* out_rgba) {
    (void)processor;
    if (out_rgba && in_rgba) {
        std::memcpy(out_rgba, in_rgba, 4 * sizeof(float));
    }
    /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Display transform                                                   */
/* ------------------------------------------------------------------ */

OakDisplayTransformHandle oak_display_transform_create(OakColorConfigHandle config,
                                                        const char* input_space,
                                                        const char* display_name,
                                                        const char* view_name,
                                                        const char* look_name,
                                                        float exposure_fstop,
                                                        float display_gamma) {
    (void)config; (void)input_space; (void)display_name; (void)view_name; (void)look_name;
    (void)exposure_fstop; (void)display_gamma;
    return nullptr; /* TODO */
}

void oak_display_transform_free(OakDisplayTransformHandle transform) {
    delete transform;
}

int oak_display_transform_apply(OakDisplayTransformHandle transform,
                                int width, int height,
                                const float* in_data, float* out_data,
                                int pix_layout) {
    (void)transform; (void)width; (void)height;
    (void)in_data; (void)out_data; (void)pix_layout;
    return -1; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Metadata & reference space                                          */
/* ------------------------------------------------------------------ */

const char* oak_color_config_reference_space_name(OakColorConfigHandle config) {
    (void)config;
    return nullptr; /* TODO */
}

bool oak_color_space_equal(OakColorSpaceHandle a, OakColorSpaceHandle b) {
    (void)a; (void)b;
    return false; /* TODO */
}
