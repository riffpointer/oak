#ifndef OAKAUDIO_INTERNAL_H
#define OAKAUDIO_INTERNAL_H

#include "oak/audio_api.h"

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

struct OakAudioBuffer {
    OakAudioParams params{};
    std::vector<float> data;
    bool interleaved = true;
    // For planar layout: per-channel pointers into data
    std::vector<float*> planar_ptrs;
};

struct OakAudioResampler {
    int src_channels = 0;
    int src_sample_rate = 0;
    int dst_channels = 0;
    int dst_sample_rate = 0;
    SwrContext* swr = nullptr;
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

struct OakAudioFilterGraph {
    AVFilterGraph* graph = nullptr;
    AVFilterContext* buffersrc_ctx = nullptr;
    AVFilterContext* buffersink_ctx = nullptr;
    AVFrame* in_frame = nullptr;
    AVFrame* out_frame = nullptr;
    OakAudioParams from_params{};
    OakAudioParams to_params{};
    AVSampleFormat from_avfmt = AV_SAMPLE_FMT_NONE;
    AVSampleFormat to_avfmt = AV_SAMPLE_FMT_NONE;
};

#endif /* OAKAUDIO_INTERNAL_H */
