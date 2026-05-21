#include "oak/audio_api.h"
#include "oakaudio_internal.h"

#include <cstring>

/* ------------------------------------------------------------------ */
/*  Audio buffer                                                        */
/* ------------------------------------------------------------------ */

OakAudioBufferHandle oak_audio_buffer_create(int channels, int64_t samples,
                                             int sample_rate, bool interleaved) {
    auto* buf = new OakAudioBuffer();
    buf->params.channels = channels;
    buf->params.duration_samples = samples;
    buf->params.sample_rate = sample_rate;
    buf->interleaved = interleaved;
    buf->data.resize(static_cast<size_t>(channels * samples));
    std::fill(buf->data.begin(), buf->data.end(), 0.0f);
    return buf;
}

void oak_audio_buffer_free(OakAudioBufferHandle buf) {
    delete buf;
}

void oak_audio_buffer_params(OakAudioBufferHandle buf, OakAudioParams* out_params) {
    if (buf && out_params) {
        *out_params = buf->params;
    }
}

void oak_audio_buffer_data(OakAudioBufferHandle buf, void** out_data, bool* out_interleaved) {
    if (!buf) return;
    if (out_data) {
        if (buf->interleaved) {
            *out_data = buf->data.data();
        } else {
            // TODO: planar layout — return array of per-channel pointers
            *out_data = buf->data.data();
        }
    }
    if (out_interleaved) *out_interleaved = buf->interleaved;
}

OakAudioBufferHandle oak_audio_buffer_clone(OakAudioBufferHandle buf) {
    if (!buf) return nullptr;
    auto* clone = new OakAudioBuffer(*buf);
    return clone;
}

/* ------------------------------------------------------------------ */
/*  Resampler                                                           */
/* ------------------------------------------------------------------ */

OakAudioResamplerHandle oak_audio_resampler_create(int src_channels, int src_sample_rate,
                                                   int dst_channels, int dst_sample_rate) {
    auto* r = new OakAudioResampler();
    r->src_channels = src_channels;
    r->src_sample_rate = src_sample_rate;
    r->dst_channels = dst_channels;
    r->dst_sample_rate = dst_sample_rate;
    // TODO: init FFmpeg swresample context
    return r;
}

void oak_audio_resampler_free(OakAudioResamplerHandle resampler) {
    delete resampler;
}

int oak_audio_resampler_process(OakAudioResamplerHandle resampler,
                                const float* in_data, int64_t in_samples,
                                float* out_data, int64_t out_samples_capacity,
                                int64_t* out_actual_samples) {
    (void)resampler; (void)in_data; (void)in_samples;
    (void)out_data; (void)out_samples_capacity;
    if (out_actual_samples) *out_actual_samples = 0;
    return -1; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Mixer                                                               */
/* ------------------------------------------------------------------ */

OakAudioMixerHandle oak_audio_mixer_create(int channels, int sample_rate) {
    auto* m = new OakAudioMixer();
    m->channels = channels;
    m->sample_rate = sample_rate;
    return m;
}

void oak_audio_mixer_free(OakAudioMixerHandle mixer) {
    delete mixer;
}

void oak_audio_mixer_add_source(OakAudioMixerHandle mixer,
                                OakAudioBufferHandle buf,
                                int64_t start_sample,
                                double volume,
                                double pan) {
    if (!mixer || !buf) return;
    OakAudioMixerSource src;
    src.buf = buf;
    src.start_sample = start_sample;
    src.volume = volume;
    src.pan = pan;
    mixer->sources.push_back(src);
}

void oak_audio_mixer_clear_sources(OakAudioMixerHandle mixer) {
    if (!mixer) return;
    mixer->sources.clear();
}

int oak_audio_mixer_mix(OakAudioMixerHandle mixer,
                        int64_t start_sample, int64_t sample_count,
                        float* out_data) {
    (void)mixer; (void)start_sample; (void)sample_count;
    if (out_data) {
        std::memset(out_data, 0,
                    static_cast<size_t>(mixer ? mixer->channels : 2) *
                    static_cast<size_t>(sample_count) * sizeof(float));
    }
    return -1; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Utility                                                             */
/* ------------------------------------------------------------------ */

int oak_audio_convert_layout(const float* in_data, int in_channels, int64_t in_samples, bool in_interleaved,
                             float* out_data, int out_channels, int64_t out_samples, bool out_interleaved) {
    (void)in_data; (void)in_channels; (void)in_samples; (void)in_interleaved;
    (void)out_data; (void)out_channels; (void)out_samples; (void)out_interleaved;
    return -1; /* TODO */
}
