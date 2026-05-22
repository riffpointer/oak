#ifndef OAKCODEC_INTERNAL_H
#define OAKCODEC_INTERNAL_H

#include "oak/codec_api.h"

#include "decoder.h"
#include "encoder.h"
#include "conformmanager.h"
#include "footagedescription.h"

namespace oakcodec
{

struct OakDecoderWrapper {
    olive::DecoderPtr decoder;
    olive::FootageDescription desc;
    QString filepath;
};

struct OakEncoderWrapper {
    olive::Encoder *encoder = nullptr;
    olive::EncodingParams params;
};

} // namespace oakcodec

#endif // OAKCODEC_INTERNAL_H
