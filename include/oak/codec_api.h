/*
 *  oakcodec.so C API
 *  媒体编解码：文件打开、视频/音频/字幕解码、编码输出、conform（音频预处理）。
 */

#ifndef OAK_CODEC_API_H
#define OAK_CODEC_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Opaque handles                                                      */
/* ------------------------------------------------------------------ */

typedef struct OakDecoder* OakDecoderHandle;
typedef struct OakEncoder* OakEncoderHandle;
typedef struct OakConform* OakConformHandle;

/* ------------------------------------------------------------------ */
/*  Pixel / audio format enums                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_PIX_FMT_INVALID = 0,
    OAK_PIX_FMT_RGBA8,       /* 8-bit unsigned per channel */
    OAK_PIX_FMT_RGBA16,      /* 16-bit unsigned per channel */
    OAK_PIX_FMT_RGBA32F,     /* 32-bit float per channel */
    OAK_PIX_FMT_RGB8,
    OAK_PIX_FMT_YUV420P8,
    OAK_PIX_FMT_YUV422P8,
    OAK_PIX_FMT_YUV444P8,
} OakPixelFormat;

typedef enum {
    OAK_AUDIO_FMT_INVALID = 0,
    OAK_AUDIO_FMT_U8,
    OAK_AUDIO_FMT_S16,
    OAK_AUDIO_FMT_S32,
    OAK_AUDIO_FMT_FLT,
    OAK_AUDIO_FMT_DBL,
} OakAudioFormat;

/* ------------------------------------------------------------------ */
/*  Stream info structs (POD)                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    int width;
    int height;
    OakPixelFormat pix_fmt;
    int64_t timebase_num;   /* 时间基分子 */
    int64_t timebase_den;   /* 时间基分母 */
    double  frame_rate;
    int64_t duration_frames;
} OakVideoStreamInfo;

typedef struct {
    int sample_rate;
    int channels;
    OakAudioFormat sample_fmt;
    int64_t timebase_num;
    int64_t timebase_den;
    int64_t duration_samples;
} OakAudioStreamInfo;

typedef struct {
    int video_stream_count;
    int audio_stream_count;
    int subtitle_stream_count;
    OakVideoStreamInfo* video_streams;   /* 数组，长度 video_stream_count */
    OakAudioStreamInfo* audio_streams;   /* 数组，长度 audio_stream_count */
} OakMediaInfo;

/* ------------------------------------------------------------------ */
/*  Decoder lifecycle                                                   */
/* ------------------------------------------------------------------ */

OakDecoderHandle oak_decoder_open(const char* filepath, const char* codec_hint,
                                  OakMediaInfo* out_info);
void             oak_decoder_close(OakDecoderHandle decoder);
void             oak_media_info_free(OakMediaInfo* info);

/* ------------------------------------------------------------------ */
/*  Video decode                                                        */
/* ------------------------------------------------------------------ */

int  oak_decoder_read_video(OakDecoderHandle decoder, int stream_index,
                            int64_t time_num, int64_t time_den,
                            OakPixelFormat out_pix_fmt,
                            int out_width, int out_height,
                            void** out_data, int* out_stride);
void oak_frame_free(void* data);

int  oak_decoder_thumbnail(OakDecoderHandle decoder, int stream_index,
                           int max_size,
                           void** out_data, int* out_width, int* out_height,
                           int* out_stride);

/* ------------------------------------------------------------------ */
/*  Audio decode & conform                                              */
/* ------------------------------------------------------------------ */

int  oak_decoder_read_audio(OakDecoderHandle decoder, int stream_index,
                            int64_t start_sample, int64_t sample_count,
                            float** out_data, int64_t* out_actual_samples);
void oak_audio_buffer_free(float* data);

/* ---- Conform (audio pre-process cache) ---- */
int  oak_conform_get(const char* decoder_id, const char* cache_path,
                     int stream_index,
                     int target_sample_rate, int target_channels,
                     bool wait,
                     const char*** out_filenames, int* out_count);
int  oak_conform_poll(const char* decoder_id);
void oak_conform_free_filenames(const char** filenames, int count);

/* ------------------------------------------------------------------ */
/*  Encoder                                                             */
/* ------------------------------------------------------------------ */

OakEncoderHandle oak_encoder_create(const char* filepath,
                                    const char* container_format,
                                    const char* video_codec,
                                    const char* audio_codec);
void oak_encoder_close(OakEncoderHandle encoder);

void oak_encoder_set_video_params(OakEncoderHandle encoder,
                                  int width, int height, OakPixelFormat pix_fmt,
                                  int64_t timebase_num, int64_t timebase_den,
                                  double frame_rate);

void oak_encoder_set_audio_params(OakEncoderHandle encoder,
                                  int sample_rate, int channels,
                                  OakAudioFormat sample_fmt,
                                  int64_t timebase_num, int64_t timebase_den);

int  oak_encoder_write_video(OakEncoderHandle encoder,
                             const void* data, int stride,
                             int64_t pts_num, int64_t pts_den);

int  oak_encoder_write_audio(OakEncoderHandle encoder,
                             const float* data, int64_t samples,
                             int64_t pts_num, int64_t pts_den);

int  oak_encoder_finalize(OakEncoderHandle encoder);

#ifdef __cplusplus
}
#endif

#endif /* OAK_CODEC_API_H */
