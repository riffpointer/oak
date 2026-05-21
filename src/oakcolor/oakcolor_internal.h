#ifndef OAKCOLOR_INTERNAL_H
#define OAKCOLOR_INTERNAL_H

#include "oak/color_api.h"

#include <string>

/* Internal C++ structures backing the C API opaque handles.
 * Will wrap OpenColorIO C++ API (OCIO::Config, OCIO::Processor, etc.). */

struct OakColorConfig {
    // TODO: OCIO::ConstConfigRcPtr config;
    std::string path;
};

struct OakColorSpace {
    // TODO: OCIO::ConstColorSpaceRcPtr space;
    std::string name;
};

struct OakColorProcessor {
    // TODO: OCIO::ConstProcessorRcPtr processor;
};

struct OakDisplayTransform {
    // TODO: OCIO::ConstProcessorRcPtr display_processor;
    float exposure_fstop = 0.0f;
    float display_gamma  = 1.0f;
};

#endif /* OAKCOLOR_INTERNAL_H */
