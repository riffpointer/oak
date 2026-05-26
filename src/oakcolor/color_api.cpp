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
        auto group = OCIO_NAMESPACE::GroupTransform::Create();

        auto dt = OCIO_NAMESPACE::DisplayViewTransform::Create();
        dt->setSrc(input_space);
        dt->setDisplay(display_name);
        dt->setView(view_name);
        group->appendTransform(dt);

        if (exposure_fstop != 0.0f || display_gamma != 1.0f) {
            auto ec = OCIO_NAMESPACE::ExposureContrastTransform::Create();
            ec->setExposure(exposure_fstop);
            ec->setGamma(display_gamma);
            group->appendTransform(ec);
        }

        auto processor = config->config->getProcessor(group);
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

/* ------------------------------------------------------------------ */
/*  High-level processor creation                                       */
/* ------------------------------------------------------------------ */

OakColorProcessorHandle oak_color_processor_create_transform(
    OakColorConfigHandle config,
    const char* src_space,
    const char* dst_space,
    int direction)
{
    if (!config || !config->config || !src_space || !dst_space) return nullptr;
    try {
        OCIO_NAMESPACE::ConstProcessorRcPtr processor;
        if (direction == 0) {
            processor = config->config->getProcessor(src_space, dst_space);
        } else {
            processor = config->config->getProcessor(dst_space, src_space);
        }
        if (!processor) return nullptr;
        auto* p = new OakColorProcessor();
        p->processor = processor;
        p->cpu = processor->getDefaultCPUProcessor();
        return p;
    } catch (...) {
        return nullptr;
    }
}

OakColorProcessorHandle oak_color_processor_create_display(
    OakColorConfigHandle config,
    const char* input_space,
    const char* display_name,
    const char* view_name,
    const char* look_name,
    int direction)
{
    if (!config || !config->config || !input_space || !display_name || !view_name)
        return nullptr;

    try {
        auto dt = OCIO_NAMESPACE::DisplayViewTransform::Create();
        dt->setSrc(input_space);
        dt->setDisplay(display_name);
        dt->setView(view_name);
        dt->setDirection(direction == 0 ?
                         OCIO_NAMESPACE::TRANSFORM_DIR_FORWARD :
                         OCIO_NAMESPACE::TRANSFORM_DIR_INVERSE);

        OCIO_NAMESPACE::ConstProcessorRcPtr processor;

        if (look_name && look_name[0]) {
            auto group = OCIO_NAMESPACE::GroupTransform::Create();

            const char* out_cs = OCIO_NAMESPACE::LookTransform::GetLooksResultColorSpace(
                config->config, config->config->getCurrentContext(), look_name);

            auto lt = OCIO_NAMESPACE::LookTransform::Create();
            lt->setSrc(input_space);
            lt->setDst(out_cs);
            lt->setLooks(look_name);
            lt->setSkipColorSpaceConversion(false);
            group->appendTransform(lt);

            dt->setSrc(out_cs);
            group->appendTransform(dt);

            processor = config->config->getProcessor(group);
        } else {
            processor = config->config->getProcessor(dt);
        }

        if (!processor) return nullptr;
        auto* p = new OakColorProcessor();
        p->processor = processor;
        p->cpu = processor->getDefaultCPUProcessor();
        return p;
    } catch (...) {
        return nullptr;
    }
}

/* ------------------------------------------------------------------ */
/*  GPU Shader generation                                               */
/* ------------------------------------------------------------------ */

OakColorGPUShaderHandle oak_color_gpu_shader_create(OakColorProcessorHandle processor,
                                                    const char* function_name,
                                                    const char* resource_prefix)
{
    if (!processor || !processor->processor) return nullptr;
    try {
        auto gpu = processor->processor->getDefaultGPUProcessor();
        if (!gpu) return nullptr;

        auto desc = OCIO_NAMESPACE::GpuShaderDesc::CreateShaderDesc();
        desc->setLanguage(OCIO_NAMESPACE::GPU_LANGUAGE_GLSL_ES_3_0);
        if (function_name && function_name[0]) {
            desc->setFunctionName(function_name);
        }
        if (resource_prefix && resource_prefix[0]) {
            desc->setResourcePrefix(resource_prefix);
        }

        gpu->extractGpuShaderInfo(desc);

        auto* s = new OakColorGPUShader();
        s->desc = desc;
        s->gpu_processor = gpu;
        s->shader_text = desc->getShaderText();
        return s;
    } catch (...) {
        return nullptr;
    }
}

void oak_color_gpu_shader_free(OakColorGPUShaderHandle shader)
{
    delete shader;
}

const char* oak_color_gpu_shader_get_text(OakColorGPUShaderHandle shader)
{
    if (!shader) return nullptr;
    return shader->shader_text.c_str();
}

int oak_color_gpu_shader_get_3d_lut_count(OakColorGPUShaderHandle shader)
{
    if (!shader || !shader->desc) return 0;
    try {
        return static_cast<int>(shader->desc->getNum3DTextures());
    } catch (...) {
        return 0;
    }
}

int oak_color_gpu_shader_get_3d_lut(OakColorGPUShaderHandle shader, int index,
                                    const char** out_name, const char** out_sampler,
                                    unsigned int* out_edge_len, int* out_interpolation,
                                    const float** out_values)
{
    if (!shader || !shader->desc || index < 0) return -1;
    try {
        const char* tex_name = nullptr;
        const char* sampler_name = nullptr;
        unsigned int edge_len = 0;
        OCIO_NAMESPACE::Interpolation interpolation = OCIO_NAMESPACE::INTERP_LINEAR;

        shader->desc->get3DTexture(index, tex_name, sampler_name, edge_len, interpolation);
        if (!tex_name || !sampler_name) return -1;

        const float* values = nullptr;
        shader->desc->get3DTextureValues(index, values);
        if (!values) return -1;

        if (out_name) *out_name = tex_name;
        if (out_sampler) *out_sampler = sampler_name;
        if (out_edge_len) *out_edge_len = edge_len;
        if (out_interpolation) {
            *out_interpolation = (interpolation == OCIO_NAMESPACE::INTERP_NEAREST) ? 0 : 1;
        }
        if (out_values) *out_values = values;
        return 0;
    } catch (...) {
        return -1;
    }
}

int oak_color_gpu_shader_get_texture_count(OakColorGPUShaderHandle shader)
{
    if (!shader || !shader->desc) return 0;
    try {
        return static_cast<int>(shader->desc->getNumTextures());
    } catch (...) {
        return 0;
    }
}

int oak_color_gpu_shader_get_texture(OakColorGPUShaderHandle shader, int index,
                                     const char** out_name, const char** out_sampler,
                                     unsigned int* out_width, unsigned int* out_height,
                                     int* out_channel_count, int* out_dimensions,
                                     int* out_interpolation,
                                     const float** out_values)
{
    if (!shader || !shader->desc || index < 0) return -1;
    try {
        const char* tex_name = nullptr;
        const char* sampler_name = nullptr;
        unsigned int width = 0, height = 0;
        OCIO_NAMESPACE::GpuShaderDesc::TextureType channel =
            OCIO_NAMESPACE::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
        OCIO_NAMESPACE::Interpolation interpolation = OCIO_NAMESPACE::INTERP_LINEAR;
        OCIO_NAMESPACE::GpuShaderDesc::TextureDimensions dimensions =
            OCIO_NAMESPACE::GpuShaderDesc::TEXTURE_2D;

        shader->desc->getTexture(index, tex_name, sampler_name, width, height,
                                 channel, dimensions, interpolation);
        if (!tex_name || !sampler_name) return -1;

        const float* values = nullptr;
        shader->desc->getTextureValues(index, values);
        if (!values) return -1;

        if (out_name) *out_name = tex_name;
        if (out_sampler) *out_sampler = sampler_name;
        if (out_width) *out_width = width;
        if (out_height) *out_height = height;
        if (out_channel_count) {
            *out_channel_count = (channel == OCIO_NAMESPACE::GpuShaderDesc::TEXTURE_RED_CHANNEL) ? 1 : 3;
        }
        if (out_dimensions) {
            *out_dimensions = (dimensions == OCIO_NAMESPACE::GpuShaderDesc::TEXTURE_1D) ? 1 : 2;
        }
        if (out_interpolation) {
            *out_interpolation = (interpolation == OCIO_NAMESPACE::INTERP_NEAREST) ? 0 : 1;
        }
        if (out_values) *out_values = values;
        return 0;
    } catch (...) {
        return -1;
    }
}

/* ------------------------------------------------------------------ */
/*  GradingPrimary transform                                            */
/* ------------------------------------------------------------------ */

OakColorGradingPrimaryHandle oak_color_grading_primary_create(int style)
{
    try {
        auto style_ocio = (style == 0) ? OCIO_NAMESPACE::GRADING_LIN : OCIO_NAMESPACE::GRADING_VIDEO;
        auto* gp = new OakColorGradingPrimary(style_ocio);
        gp->transform = OCIO_NAMESPACE::GradingPrimaryTransform::Create(style_ocio);
        return gp;
    } catch (...) {
        return nullptr;
    }
}

void oak_color_grading_primary_free(OakColorGradingPrimaryHandle gp)
{
    delete gp;
}

void oak_color_grading_primary_set_dynamic(OakColorGradingPrimaryHandle gp, bool dynamic)
{
    if (!gp || !gp->transform) return;
    try {
        gp->transform->makeDynamic();
    } catch (...) {}
}

void oak_color_grading_primary_set_direction(OakColorGradingPrimaryHandle gp, int direction)
{
    if (!gp || !gp->transform) return;
    try {
        gp->transform->setDirection(direction == 0 ?
                                    OCIO_NAMESPACE::TRANSFORM_DIR_FORWARD :
                                    OCIO_NAMESPACE::TRANSFORM_DIR_INVERSE);
    } catch (...) {}
}

void oak_color_grading_primary_set_contrast(OakColorGradingPrimaryHandle gp, const float* rgbm)
{
    if (!gp || !rgbm) return;
    gp->values.m_contrast.m_red = rgbm[0];
    gp->values.m_contrast.m_green = rgbm[1];
    gp->values.m_contrast.m_blue = rgbm[2];
    gp->values.m_contrast.m_master = rgbm[3];
    try {
        if (gp->transform) gp->transform->setValue(gp->values);
    } catch (...) {}
}

void oak_color_grading_primary_set_offset(OakColorGradingPrimaryHandle gp, const float* rgbm)
{
    if (!gp || !rgbm) return;
    gp->values.m_offset.m_red = rgbm[0];
    gp->values.m_offset.m_green = rgbm[1];
    gp->values.m_offset.m_blue = rgbm[2];
    gp->values.m_offset.m_master = rgbm[3];
    try {
        if (gp->transform) gp->transform->setValue(gp->values);
    } catch (...) {}
}

void oak_color_grading_primary_set_exposure(OakColorGradingPrimaryHandle gp, const float* rgbm)
{
    if (!gp || !rgbm) return;
    gp->values.m_exposure.m_red = rgbm[0];
    gp->values.m_exposure.m_green = rgbm[1];
    gp->values.m_exposure.m_blue = rgbm[2];
    gp->values.m_exposure.m_master = rgbm[3];
    try {
        if (gp->transform) gp->transform->setValue(gp->values);
    } catch (...) {}
}

void oak_color_grading_primary_set_saturation(OakColorGradingPrimaryHandle gp, float val)
{
    if (!gp) return;
    gp->values.m_saturation = val;
    try {
        if (gp->transform) gp->transform->setValue(gp->values);
    } catch (...) {}
}

void oak_color_grading_primary_set_pivot(OakColorGradingPrimaryHandle gp, float val)
{
    if (!gp) return;
    gp->values.m_pivot = val;
    try {
        if (gp->transform) gp->transform->setValue(gp->values);
    } catch (...) {}
}

void oak_color_grading_primary_set_clamp_black(OakColorGradingPrimaryHandle gp, float val)
{
    if (!gp) return;
    gp->values.m_clampBlack = val;
    try {
        if (gp->transform) gp->transform->setValue(gp->values);
    } catch (...) {}
}

void oak_color_grading_primary_set_clamp_white(OakColorGradingPrimaryHandle gp, float val)
{
    if (!gp) return;
    gp->values.m_clampWhite = val;
    try {
        if (gp->transform) gp->transform->setValue(gp->values);
    } catch (...) {}
}

float oak_color_grading_primary_no_clamp_black(void)
{
    return OCIO_NAMESPACE::GradingPrimary::NoClampBlack();
}

float oak_color_grading_primary_no_clamp_white(void)
{
    return OCIO_NAMESPACE::GradingPrimary::NoClampWhite();
}

OakColorProcessorHandle oak_color_processor_create_from_grading(
    OakColorConfigHandle config,
    const char* input_space,
    const char* output_space,
    OakColorGradingPrimaryHandle gp,
    int direction)
{
    if (!config || !config->config || !gp || !gp->transform) return nullptr;
    try {
        gp->transform->setDirection(direction == 0 ?
                                    OCIO_NAMESPACE::TRANSFORM_DIR_FORWARD :
                                    OCIO_NAMESPACE::TRANSFORM_DIR_INVERSE);
        auto processor = config->config->getProcessor(gp->transform);
        if (!processor) return nullptr;
        auto* p = new OakColorProcessor();
        p->processor = processor;
        p->cpu = processor->getDefaultCPUProcessor();
        return p;
    } catch (...) {
        return nullptr;
    }
}
