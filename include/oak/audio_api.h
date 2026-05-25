/*
 *  oakaudio.so C API
 *  音频处理：混合、重采样、格式转换、效果链。
 */

#ifndef OAK_AUDIO_API_H
#define OAK_AUDIO_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Audio format                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_AUDIO_FMT_INVALID = 0,
    OAK_AUDIO_FMT_U8,
    OAK_AUDIO_FMT_S16,
    OAK_AUDIO_FMT_S32,
    OAK_AUDIO_FMT_FLT,
    OAK_AUDIO_FMT_DBL,
} OakAudioFormat;

/* ------------------------------------------------------------------ */
/*  Opaque handles                                                      */
/* ------------------------------------------------------------------ */

typedef struct OakAudioMixer*     OakAudioMixerHandle;
typedef struct OakAudioResampler* OakAudioResamplerHandle;
typedef struct OakAudioBuffer*    OakAudioBufferHandle;

/* ------------------------------------------------------------------ */
/*  Audio params (POD)                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    int sample_rate;
    int channels;
    int64_t duration_samples;
    OakAudioFormat sample_fmt;
    uint64_t channel_layout_mask;
} OakAudioParams;

/* ------------------------------------------------------------------ */
/*  Audio buffer                                                        */
/* ------------------------------------------------------------------ */

OakAudioBufferHandle oak_audio_buffer_create(int channels, int64_t samples,
                                             int sample_rate, bool interleaved);
void                 oak_audio_buffer_free(OakAudioBufferHandle buf);

void oak_audio_buffer_params(OakAudioBufferHandle buf, OakAudioParams* out_params);
void oak_audio_buffer_data(OakAudioBufferHandle buf, void** out_data, bool* out_interleaved);

OakAudioBufferHandle oak_audio_buffer_clone(OakAudioBufferHandle buf);

/* ------------------------------------------------------------------ */
/*  Resampler                                                           */
/* ------------------------------------------------------------------ */

OakAudioResamplerHandle oak_audio_resampler_create(int src_channels, int src_sample_rate,
                                                   int dst_channels, int dst_sample_rate);
void                    oak_audio_resampler_free(OakAudioResamplerHandle resampler);

int oak_audio_resampler_process(OakAudioResamplerHandle resampler,
                                const float* in_data, int64_t in_samples,
                                float* out_data, int64_t out_samples_capacity,
                                int64_t* out_actual_samples);

/* ------------------------------------------------------------------ */
/*  Mixer                                                               */
/* ------------------------------------------------------------------ */

OakAudioMixerHandle oak_audio_mixer_create(int channels, int sample_rate);
void                oak_audio_mixer_free(OakAudioMixerHandle mixer);

void oak_audio_mixer_add_source(OakAudioMixerHandle mixer,
                                OakAudioBufferHandle buf,
                                int64_t start_sample,
                                double volume,
                                double pan);

void oak_audio_mixer_clear_sources(OakAudioMixerHandle mixer);

int  oak_audio_mixer_mix(OakAudioMixerHandle mixer,
                         int64_t start_sample, int64_t sample_count,
                         float* out_data);

/* ------------------------------------------------------------------ */
/*  Audio Filter Graph (FFmpeg avfilter)                                */
/* ------------------------------------------------------------------ */

typedef struct OakAudioFilterGraph* OakAudioFilterGraphHandle;

OakAudioFilterGraphHandle oak_audio_filter_graph_create(const OakAudioParams* from,
                                                        const OakAudioParams* to,
                                                        double tempo);
void                      oak_audio_filter_graph_destroy(OakAudioFilterGraphHandle graph);

int oak_audio_filter_graph_process(OakAudioFilterGraphHandle graph,
                                   const float** in_data, int64_t in_samples,
                                   float** out_data, int64_t* out_samples,
                                   int* out_channels);

int oak_audio_filter_graph_flush(OakAudioFilterGraphHandle graph,
                                 float** out_data, int64_t* out_samples,
                                 int* out_channels);

void oak_audio_filter_graph_free_output(float* data);

/* ------------------------------------------------------------------ */
/*  Utility                                                             */
/* ------------------------------------------------------------------ */

int oak_audio_convert_layout(const float* in_data, int in_channels, int64_t in_samples, bool in_interleaved,
                             float* out_data, int out_channels, int64_t out_samples, bool out_interleaved);

#ifdef __cplusplus
}
#endif

#endif /* OAK_AUDIO_API_H */
