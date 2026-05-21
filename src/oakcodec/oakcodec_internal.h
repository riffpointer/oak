#ifndef OAKCODEC_INTERNAL_H
#define OAKCODEC_INTERNAL_H

#include "oak/codec_api.h"

#include <string>

/* Internal C++ structures backing the C API opaque handles.
 * Will wrap existing Decoder/Encoder/ConformManager classes. */

struct OakDecoder {
    std::string filepath;
    // TODO: wrap existing olive::Decoder or FFmpeg decoder
};

struct OakEncoder {
    std::string filepath;
    // TODO: wrap existing olive::Encoder
};

struct OakConform {
    // TODO: wrap ConformManager / ConformTask
};

#endif /* OAKCODEC_INTERNAL_H */
