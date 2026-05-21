#include "oak/codec_api.h"
#include "oakcodec_internal.h"

#include <cstdlib>
#include <cstring>

/* ------------------------------------------------------------------ */
/*  Decoder lifecycle                                                   */
/* ------------------------------------------------------------------ */

OakDecoderHandle oak_decoder_open(const char* filepath, const char* codec_hint,
                                  OakMediaInfo* out_info) {
    (void)codec_hint;
    if (!filepath) return nullptr;
    // TODO: open file with FFmpeg/OIIO, fill out_info
    if (out_info) {
        std::memset(out_info, 0, sizeof(*out_info));
    }
    return nullptr; /* TODO */
}

void oak_decoder_close(OakDecoderHandle decoder) {
    delete decoder;
}

void oak_media_info_free(OakMediaInfo* info) {
    if (!info) return;
    if (info->video_streams) {
        std::free(info->video_streams);
        info->video_streams = nullptr;
    }
    if (info->audio_streams) {
        std::free(info->audio_streams);
        info->audio_streams = nullptr;
    }
    info->video_stream_count = 0;
    info->audio_stream_count = 0;
}

/* ------------------------------------------------------------------ */
/*  Video decode                                                        */
/* ------------------------------------------------------------------ */

int oak_decoder_read_video(OakDecoderHandle decoder, int stream_index,
                           int64_t time_num, int64_t time_den,
                           OakPixelFormat out_pix_fmt,
                           int out_width, int out_height,
                           void** out_data, int* out_stride) {
    (void)decoder; (void)stream_index; (void)time_num; (void)time_den;
    (void)out_pix_fmt; (void)out_width; (void)out_height;
    if (out_data)   *out_data   = nullptr;
    if (out_stride) *out_stride = 0;
    return -1; /* TODO */
}

void oak_frame_free(void* data) {
    std::free(data);
}

int oak_decoder_thumbnail(OakDecoderHandle decoder, int stream_index,
                          int max_size,
                          void** out_data, int* out_width, int* out_height,
                          int* out_stride) {
    (void)decoder; (void)stream_index; (void)max_size;
    if (out_data)   *out_data   = nullptr;
    if (out_width)  *out_width  = 0;
    if (out_height) *out_height = 0;
    if (out_stride) *out_stride = 0;
    return -1; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Audio decode & conform                                              */
/* ------------------------------------------------------------------ */

int oak_decoder_read_audio(OakDecoderHandle decoder, int stream_index,
                           int64_t start_sample, int64_t sample_count,
                           float** out_data, int64_t* out_actual_samples) {
    (void)decoder; (void)stream_index; (void)start_sample; (void)sample_count;
    if (out_data)          *out_data          = nullptr;
    if (out_actual_samples)*out_actual_samples = 0;
    return -1; /* TODO */
}

void oak_audio_buffer_free(float* data) {
    std::free(data);
}

int oak_conform_get(const char* decoder_id, const char* cache_path,
                    int stream_index,
                    int target_sample_rate, int target_channels,
                    bool wait,
                    const char*** out_filenames, int* out_count) {
    (void)decoder_id; (void)cache_path; (void)stream_index;
    (void)target_sample_rate; (void)target_channels; (void)wait;
    if (out_filenames) *out_filenames = nullptr;
    if (out_count)     *out_count     = 0;
    return -1; /* TODO */
}

int oak_conform_poll(const char* decoder_id) {
    (void)decoder_id;
    return -1; /* TODO */
}

void oak_conform_free_filenames(const char** filenames, int count) {
    if (!filenames) return;
    for (int i = 0; i < count; ++i) {
        std::free((void*)filenames[i]);
    }
    std::free((void*)filenames);
}

/* ------------------------------------------------------------------ */
/*  Encoder                                                             */
/* ------------------------------------------------------------------ */

OakEncoderHandle oak_encoder_create(const char* filepath,
                                    const char* container_format,
                                    const char* video_codec,
                                    const char* audio_codec) {
    (void)filepath; (void)container_format; (void)video_codec; (void)audio_codec;
    return nullptr; /* TODO */
}

void oak_encoder_close(OakEncoderHandle encoder) {
    delete encoder;
}

void oak_encoder_set_video_params(OakEncoderHandle encoder,
                                  int width, int height, OakPixelFormat pix_fmt,
                                  int64_t timebase_num, int64_t timebase_den,
                                  double frame_rate) {
    (void)encoder; (void)width; (void)height; (void)pix_fmt;
    (void)timebase_num; (void)timebase_den; (void)frame_rate;
    /* TODO */
}

void oak_encoder_set_audio_params(OakEncoderHandle encoder,
                                  int sample_rate, int channels,
                                  OakAudioFormat sample_fmt,
                                  int64_t timebase_num, int64_t timebase_den) {
    (void)encoder; (void)sample_rate; (void)channels; (void)sample_fmt;
    (void)timebase_num; (void)timebase_den;
    /* TODO */
}

int oak_encoder_write_video(OakEncoderHandle encoder,
                            const void* data, int stride,
                            int64_t pts_num, int64_t pts_den) {
    (void)encoder; (void)data; (void)stride; (void)pts_num; (void)pts_den;
    return -1; /* TODO */
}

int oak_encoder_write_audio(OakEncoderHandle encoder,
                            const float* data, int64_t samples,
                            int64_t pts_num, int64_t pts_den) {
    (void)encoder; (void)data; (void)samples; (void)pts_num; (void)pts_den;
    return -1; /* TODO */
}

int oak_encoder_finalize(OakEncoderHandle encoder) {
    (void)encoder;
    return -1; /* TODO */
}
