/***  Oak Video Editor - Codec Runtime Loader  Copyright (C) 2025 mikesolar  ***/

#ifndef OAK_CODEC_RUNTIME_H
#define OAK_CODEC_RUNTIME_H

#include "runtime_loader.h"
#include "oak/codec_api.h"

namespace olive {

class OakCodecRuntime : public OakRuntimeLoader {
public:
    static OakCodecRuntime* Instance();

    bool Load();

    /* --- decoder lifecycle --- */
    OakDecoderHandle (*decoder_open)(const char* filepath, const char* codec_hint,
                                     OakMediaInfo* out_info) = nullptr;
    void (*decoder_close)(OakDecoderHandle decoder) = nullptr;
    void (*media_info_free)(OakMediaInfo* info) = nullptr;

    OakDecoderHandle (*decoder_create_from_id)(const char* id) = nullptr;
    const char* (*decoder_id)(OakDecoderHandle decoder) = nullptr;
    int (*decoder_supports_video)(OakDecoderHandle decoder) = nullptr;
    int (*decoder_supports_audio)(OakDecoderHandle decoder) = nullptr;
    int (*decoder_is_open)(OakDecoderHandle decoder) = nullptr;
    void (*decoder_set_progress_callback)(OakDecoderHandle decoder,
                                          void (*cb)(double, void*), void* userdata) = nullptr;

    OakMediaInfo* (*decoder_probe_file)(OakDecoderHandle decoder, const char* filepath) = nullptr;
    int (*decoder_open_stream)(OakDecoderHandle decoder, const char* filepath, int stream_index) = nullptr;

    int (*decoder_read_video)(OakDecoderHandle decoder, int stream_index,
                              int64_t time_num, int64_t time_den,
                              void* renderer_hint, OakFrame* out_frame) = nullptr;
    int (*decoder_thumbnail)(OakDecoderHandle decoder, int stream_index,
                             int max_size, OakFrame* out_frame) = nullptr;
    int (*decoder_read_video_ex)(OakDecoderHandle decoder, int stream_index,
                                 const OakDecoderVideoParams* params, OakFrame* out_frame) = nullptr;

    int (*decoder_read_audio)(OakDecoderHandle decoder, int stream_index,
                              int64_t start_sample, int64_t sample_count,
                              float** out_data, int64_t* out_actual_samples) = nullptr;
    void (*audio_buffer_free)(float* data) = nullptr;
    int (*decoder_read_audio_ex)(OakDecoderHandle decoder, int stream_index,
                                 const OakDecoderAudioParams* params,
                                 float** out_data, int64_t* out_actual_samples) = nullptr;

    int (*decoder_conform_audio)(OakDecoderHandle decoder, const char* cache_path,
                                 int target_sample_rate, int target_channels,
                                 OakAudioFormat target_sample_fmt) = nullptr;

    int (*conform_get)(const char* filename, const char* decoder_id,
                       const char* cache_path, int stream_index,
                       int target_sample_rate, int target_channels,
                       OakAudioFormat target_sample_fmt,
                       bool wait,
                       const char*** out_filenames, int* out_count) = nullptr;
    int (*conform_poll)(const char* filename, const char* cache_path,
                        int stream_index,
                        int target_sample_rate, int target_channels,
                        OakAudioFormat target_sample_fmt) = nullptr;
    void (*conform_free_filenames)(const char** filenames, int count) = nullptr;
    void (*conform_set_ready_callback)(void (*cb)(void*), void* userdata) = nullptr;

    /* --- encoder --- */
    OakEncoderHandle (*encoder_create)(const char* filepath,
                                       const char* container_format,
                                       const char* video_codec,
                                       const char* audio_codec) = nullptr;
    void (*encoder_close)(OakEncoderHandle encoder) = nullptr;
    void (*encoder_set_video_params)(OakEncoderHandle encoder,
                                     int width, int height,
                                     OakFramePixelFormat pix_fmt,
                                     int64_t timebase_num, int64_t timebase_den,
                                     double frame_rate) = nullptr;
    void (*encoder_set_video_output_format)(OakEncoderHandle encoder,
                                            OakFramePixelFormat output_pix_fmt) = nullptr;
    void (*encoder_set_video_output_colorspace)(OakEncoderHandle encoder,
                                                const char* output_colorspace) = nullptr;
    void (*encoder_set_audio_params)(OakEncoderHandle encoder,
                                     int sample_rate, int channels,
                                     OakAudioFormat sample_fmt,
                                     int64_t timebase_num, int64_t timebase_den) = nullptr;
    int (*encoder_write_video)(OakEncoderHandle encoder, const OakFrame* frame) = nullptr;
    int (*encoder_write_audio)(OakEncoderHandle encoder,
                               const float* data, int64_t samples,
                               int64_t pts_num, int64_t pts_den) = nullptr;
    int (*encoder_finalize)(OakEncoderHandle encoder) = nullptr;

    /* --- frame utilities (for PluginRenderer) --- */
    void* (*frame_alloc)(int width, int height, int av_format) = nullptr;
    void (*frame_free)(void* frame) = nullptr;
    int (*frame_get_plane)(void* frame, int plane, void** out_data, int* out_linesize) = nullptr;
    int (*frame_get_params)(void* frame, int* out_width, int* out_height, int* out_av_format) = nullptr;
    int (*frame_convert)(void* src_frame, void* dst_frame) = nullptr;
    int (*video_format_to_av)(int pixel_format, int channel_count) = nullptr;
    int (*av_to_video_format)(int av_format, int* out_pixel_format, int* out_channel_count) = nullptr;
    int (*video_format_is_planar)(int av_format) = nullptr;
    int (*video_format_compatible)(int pixel_format) = nullptr;

private:
    OakCodecRuntime() = default;
};

} // namespace olive

#endif // OAK_CODEC_RUNTIME_H
