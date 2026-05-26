/***

  oakcodec.so — AudioParams helpers for FFmpeg interop
  Copyright (C) 2025 mikesolar

***/

#ifndef OAKCODEC_AUDIO_PARAMS_HELPERS_H
#define OAKCODEC_AUDIO_PARAMS_HELPERS_H

#include <olive/core/core.h>

extern "C" {
#include <libavutil/channel_layout.h>
}

namespace oakcodec {

/**
 * @brief Convert OakShared AudioParams (uint64_t mask) to FFmpeg AVChannelLayout.
 *
 * The caller must call av_channel_layout_uninit(&out) when done.
 */
inline void AudioParamsToAVChannelLayout(const olive::core::AudioParams &params,
                                          AVChannelLayout *out)
{
    av_channel_layout_from_mask(out, params.channel_layout_mask());
}

}

#endif // OAKCODEC_AUDIO_PARAMS_HELPERS_H
