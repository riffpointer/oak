#ifndef OAKCOLOR_INTERNAL_H
#define OAKCOLOR_INTERNAL_H

#include "oak/color_api.h"

#include <string>
#include <memory>

#include <OpenColorIO/OpenColorIO.h>

struct OakColorConfig {
    OCIO_NAMESPACE::ConstConfigRcPtr config;
    std::string path;
};

struct OakColorSpace {
    OCIO_NAMESPACE::ConstColorSpaceRcPtr space;
    std::string name;
};

struct OakColorProcessor {
    OCIO_NAMESPACE::ConstProcessorRcPtr processor;
    OCIO_NAMESPACE::ConstCPUProcessorRcPtr cpu;
};

struct OakDisplayTransform {
    OCIO_NAMESPACE::ConstProcessorRcPtr processor;
    OCIO_NAMESPACE::ConstCPUProcessorRcPtr cpu;
    float exposure_fstop = 0.0f;
    float display_gamma  = 1.0f;
};

struct OakColorGPUShader {
    OCIO_NAMESPACE::GpuShaderDescRcPtr desc;
    OCIO_NAMESPACE::ConstGPUProcessorRcPtr gpu_processor;
    std::string shader_text;
};

struct OakColorGradingPrimary {
    OCIO_NAMESPACE::GradingPrimaryTransformRcPtr transform;
    OCIO_NAMESPACE::GradingPrimary values;

    OakColorGradingPrimary(OCIO_NAMESPACE::GradingStyle style)
        : values(style) {}
};

#endif /* OAKCOLOR_INTERNAL_H */
