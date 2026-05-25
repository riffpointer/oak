#ifndef OAKCODEC_INTERNAL_H
#define OAKCODEC_INTERNAL_H

#include "oak/codec_api.h"
#include "oak/color_api.h"

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
    bool opened = false;
    QByteArray cached_id;

    // Cached color management objects for IDT
    OakColorConfigHandle color_config = nullptr;
    OakColorProcessorHandle color_processor = nullptr;
    QString cached_src_colorspace;

    // Progress callback
    OakDecoderProgressCallback progress_cb = nullptr;
    void* progress_userdata = nullptr;
};

struct OakEncoderWrapper {
    olive::Encoder *encoder = nullptr;
    olive::EncodingParams params;
    OakFramePixelFormat output_pix_fmt = OAK_FRAME_PIX_INVALID;
    QString output_colorspace;

    // Cached color management objects
    OakColorConfigHandle color_config = nullptr;
    OakColorProcessorHandle color_processor = nullptr;
    QString cached_src_colorspace;
};

} // namespace oakcodec

#endif // OAKCODEC_INTERNAL_H
