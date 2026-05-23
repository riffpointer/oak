/***

  oakcolor.so C API Implementation (v1 — OpenColorIO 2.x)
  Copyright (C) 2025 mikesolar

***/

#include "oak/color_api.h"
#include "oakcolor_internal.h"

#include <OpenColorIO/OpenColorIO.h>
#include <cstring>
#include <cstdlib>

/* ------------------------------------------------------------------ */
/*  Config management                                                   */
/* ------------------------------------------------------------------ */

OakColorConfigHandle oak_color_config_load(const char* config_path)
{
    try {
        auto* cfg = new OakColorConfig();
        if (config_path && config_path[0]) {
            cfg->config = OCIO_NAMESPACE::Config::CreateFromFile(config_path);
            cfg->path = config_path;
        } else {
            // Try environment variable OCIO, fallback to built-in raw
            const char* env = std::getenv("OCIO");
            if (env && env[0]) {
                cfg->config = OCIO_NAMESPACE::Config::CreateFromFile(env);
                cfg->path = env;
            } else {
                cfg->config = OCIO_NAMESPACE::Config::CreateRaw();
                cfg->path = "raw";
            }
        }
        return cfg;
    } catch (...) {
        return nullptr;
    }
}

void oak_color_config_free(OakColorConfigHandle config)
{
    delete config;
}

int oak_color_config_space_count(OakColorConfigHandle config)
{
    if (!config || !config->config) return 0;
    try {
        return config->config->getNumColorSpaces(OCIO_NAMESPACE::SEARCH_REFERENCE_SPACE_ALL,
                                                  OCIO_NAMESPACE::COLORSPACE_ALL);
    } catch (...) {
        return 0;
    }
}

const char* oak_color_config_space_name(OakColorConfigHandle config, int index)
{
    if (!config || !config->config) return nullptr;
    try {
        return config->config->getColorSpaceNameByIndex(
            OCIO_NAMESPACE::SEARCH_REFERENCE_SPACE_ALL,
            OCIO_NAMESPACE::COLORSPACE_ALL, index);
    } catch (...) {
        return nullptr;
    }
}

OakColorSpaceHandle oak_color_config_get_space(OakColorConfigHandle config, const char* name)
{
    if (!config || !config->config || !name) return nullptr;
    try {
        auto cs = config->config->getColorSpace(name);
        if (!cs) return nullptr;
        auto* s = new OakColorSpace();
        s->space = cs;
        s->name = name;
        return s;
    } catch (...) {
        return nullptr;
    }
}

const char* oak_color_config_default_input_space(OakColorConfigHandle config)
{
    if (!config || !config->config) return nullptr;
    try {
        // Try scene_linear role first
        const char* role = config->config->getRoleColorSpace("scene_linear");
        if (role && role[0]) return role;
        // Fallback to ACES - ACEScg
        auto cs = config->config->getColorSpace("ACES - ACEScg");
        if (cs) return "ACES - ACEScg";
        // Fallback to first space
        return config->config->getColorSpaceNameByIndex(
            OCIO_NAMESPACE::SEARCH_REFERENCE_SPACE_ALL,
            OCIO_NAMESPACE::COLORSPACE_ALL, 0);
    } catch (...) {
        return nullptr;
    }
}

const char* oak_color_config_default_display(OakColorConfigHandle config)
{
    if (!config || !config->config) return nullptr;
    try {
        return config->config->getDefaultDisplay();
    } catch (...) {
        return nullptr;
    }
}

int oak_color_config_display_view_count(OakColorConfigHandle config, const char* display_name)
{
    if (!config || !config->config) return 0;
    try {
        return config->config->getNumViews(display_name);
    } catch (...) {
        return 0;
    }
}

const char* oak_color_config_display_view_name(OakColorConfigHandle config,
                                               const char* display_name, int view_index)
{
    if (!config || !config->config) return nullptr;
    try {
        return config->config->getView(display_name, view_index);
    } catch (...) {
        return nullptr;
    }
}

/* ------------------------------------------------------------------ */
/*  Color processor                                                     */
/* ------------------------------------------------------------------ */

OakColorProcessorHandle oak_color_processor_create(OakColorConfigHandle config,
                                                   const char* src_space_name,
                                                   const char* dst_space_name)
{
    if (!config || !config->config || !src_space_name || !dst_space_name) return nullptr;
    try {
        auto processor = config->config->getProcessor(src_space_name, dst_space_name);
        if (!processor) return nullptr;
        auto* p = new OakColorProcessor();
        p->processor = processor;
        p->cpu = processor->getDefaultCPUProcessor();
        return p;
    } catch (...) {
        return nullptr;
    }
}

OakColorProcessorHandle oak_color_processor_create_from_lut(const char* lut_path,
                                                            const char* input_space,
                                                            const char* output_space)
{
    (void)lut_path; (void)input_space; (void)output_space;
    return nullptr; /* TODO: FileTransform + Config::getProcessor */
}

void oak_color_processor_free(OakColorProcessorHandle processor)
{
    delete processor;
}

int oak_color_processor_apply(OakColorProcessorHandle processor,
                              int width, int height,
                              const float* in_data, float* out_data,
                              int pix_layout)
{
    (void)pix_layout;
    if (!processor || !processor->cpu || width <= 0 || height <= 0) return -1;
    if (!in_data || !out_data) return -1;

    try {
        // Assume RGBA interleaved float (4 channels)
        OCIO_NAMESPACE::PackedImageDesc imgDesc(
            const_cast<float*>(in_data), width, height, 4);
        OCIO_NAMESPACE::PackedImageDesc outDesc(out_data, width, height, 4);

        processor->cpu->apply(imgDesc, outDesc);
        return 0;
    } catch (...) {
        return -1;
    }
}

void oak_color_processor_apply_pixel(OakColorProcessorHandle processor,
                                     const float* in_rgba, float* out_rgba)
{
    if (!processor || !processor->cpu || !in_rgba || !out_rgba) {
        if (out_rgba && in_rgba) {
            std::memcpy(out_rgba, in_rgba, 4 * sizeof(float));
        }
        return;
    }
    try {
        float in[4];
        std::memcpy(in, in_rgba, sizeof(in));
        OCIO_NAMESPACE::PackedImageDesc inDesc(in, 1, 1, 4);
        OCIO_NAMESPACE::PackedImageDesc outDesc(out_rgba, 1, 1, 4);
        processor->cpu->apply(inDesc, outDesc);
    } catch (...) {
        std::memcpy(out_rgba, in_rgba, 4 * sizeof(float));
    }
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
                                                        float display_gamma)
{
    (void)look_name;
    if (!config || !config->config || !input_space || !display_name || !view_name)
        return nullptr;
    try {
        auto dt = OCIO_NAMESPACE::DisplayViewTransform::Create();
        dt->setSrc(input_space);
        dt->setDisplay(display_name);
        dt->setView(view_name);

        auto processor = config->config->getProcessor(dt);
        if (!processor) return nullptr;

        auto* t = new OakDisplayTransform();
        t->processor = processor;
        t->cpu = processor->getDefaultCPUProcessor();
        t->exposure_fstop = exposure_fstop;
        t->display_gamma = display_gamma;
        return t;
    } catch (...) {
        return nullptr;
    }
}

void oak_display_transform_free(OakDisplayTransformHandle transform)
{
    delete transform;
}

int oak_display_transform_apply(OakDisplayTransformHandle transform,
                                int width, int height,
                                const float* in_data, float* out_data,
                                int pix_layout)
{
    (void)pix_layout;
    if (!transform || !transform->cpu || width <= 0 || height <= 0) return -1;
    if (!in_data || !out_data) return -1;

    try {
        OCIO_NAMESPACE::PackedImageDesc inDesc(
            const_cast<float*>(in_data), width, height, 4);
        OCIO_NAMESPACE::PackedImageDesc outDesc(out_data, width, height, 4);
        transform->cpu->apply(inDesc, outDesc);
        return 0;
    } catch (...) {
        return -1;
    }
}

/* ------------------------------------------------------------------ */
/*  Metadata & reference space                                          */
/* ------------------------------------------------------------------ */

const char* oak_color_config_reference_space_name(OakColorConfigHandle config)
{
    if (!config || !config->config) return nullptr;
    try {
        return config->config->getRoleColorSpace("scene_linear");
    } catch (...) {
        return nullptr;
    }
}

bool oak_color_space_equal(OakColorSpaceHandle a, OakColorSpaceHandle b)
{
    if (!a || !b) return false;
    return a->name == b->name;
}
