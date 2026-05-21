#ifndef OAKAUDIO_INTERNAL_H
#define OAKAUDIO_INTERNAL_H

#include "oak/audio_api.h"

#include <vector>
#include <cstdint>

/* Internal C++ structures backing the C API opaque handles. */

struct OakAudioBuffer {
    OakAudioParams params{};
    std::vector<float> data;
    bool interleaved = true;
};

struct OakAudioResampler {
    int src_channels = 0;
    int src_sample_rate = 0;
    int dst_channels = 0;
    int dst_sample_rate = 0;
    // TODO: wrap FFmpeg swresample or similar
};

struct OakAudioMixerSource {
    OakAudioBufferHandle buf = nullptr;
    int64_t start_sample = 0;
    double volume = 1.0;
    double pan = 0.0;
};

struct OakAudioMixer {
    int channels = 0;
    int sample_rate = 0;
    std::vector<OakAudioMixerSource> sources;
};

#endif /* OAKAUDIO_INTERNAL_H */
