/*
 *  oakcodec.so C API (v2)
 *  媒体编解码：文件打开、视频/音频/字幕解码、编码输出、conform。
 *  FFmpeg 调用完全封装在 oakcodec.so 内部，外部不得直接调用 FFmpeg。
 *  全链路默认：RGBA32F + ACEScg。
 */

#ifndef OAK_CODEC_API_H
#define OAK_CODEC_API_H

#include "oak/frame_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakDecoder*  OakDecoderHandle;
typedef struct OakEncoder*  OakEncoderHandle;
typedef struct OakConform*  OakConformHandle;

/* ------------------------------------------------------------------ */
/*  音频格式                                                            */
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
/*  流信息（POD）                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    int     width;
    int     height;
    OakFramePixelFormat pix_fmt;
    int64_t timebase_num;
    int64_t timebase_den;
    double  frame_rate;
    int64_t duration_frames;
} OakVideoStreamInfo;

typedef struct {
    int     sample_rate;
    int     channels;
    OakAudioFormat sample_fmt;
    int64_t timebase_num;
    int64_t timebase_den;
    int64_t duration_samples;
} OakAudioStreamInfo;

typedef struct {
    int video_stream_count;
    int audio_stream_count;
    int subtitle_stream_count;
    OakVideoStreamInfo* video_streams;
    OakAudioStreamInfo* audio_streams;
} OakMediaInfo;

/* ------------------------------------------------------------------ */
/*  解码器生命周期                                                      */
/* ------------------------------------------------------------------ */

OakDecoderHandle oak_decoder_open(const char* filepath, const char* codec_hint,
                                  OakMediaInfo* out_info);
void             oak_decoder_close(OakDecoderHandle decoder);
void             oak_media_info_free(OakMediaInfo* info);

/* ------------------------------------------------------------------ */
/*  解码器创建与能力查询                                                */
/* ------------------------------------------------------------------ */

OakDecoderHandle oak_decoder_create_from_id(const char* id);
const char*      oak_decoder_id(OakDecoderHandle decoder);

/* progress callback: called during indexing/conform */
typedef void (*OakDecoderProgressCallback)(double progress, void* userdata);
void oak_decoder_set_progress_callback(OakDecoderHandle decoder,
                                       OakDecoderProgressCallback cb,
                                       void* userdata);
int              oak_decoder_supports_video(OakDecoderHandle decoder);
int              oak_decoder_supports_audio(OakDecoderHandle decoder);
int              oak_decoder_is_open(OakDecoderHandle decoder);

/* ------------------------------------------------------------------ */
/*  文件探测                                                            */
/* ------------------------------------------------------------------ */

OakMediaInfo* oak_decoder_probe_file(OakDecoderHandle decoder, const char* filepath);

/* ------------------------------------------------------------------ */
/*  流打开                                                              */
/* ------------------------------------------------------------------ */

int oak_decoder_open_stream(OakDecoderHandle decoder, const char* filepath, int stream_index);

/* ------------------------------------------------------------------ */
/*  视频解码（返回 OakFrame）                                           */
/* ------------------------------------------------------------------ */

int  oak_decoder_read_video(OakDecoderHandle decoder, int stream_index,
                            int64_t time_num, int64_t time_den,
                            void* renderer_hint,   /* OakRendererHandle cast to void*, NULL = CPU */
                            OakFrame* out_frame);

int  oak_decoder_thumbnail(OakDecoderHandle decoder, int stream_index,
                           int max_size,
                           OakFrame* out_frame);

/* ------------------------------------------------------------------ */
/*  扩展视频解码参数                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    int64_t time_num;
    int64_t time_den;
    int     divider;
    int     maximum_format;  /* OakFramePixelFormat */
    int     force_range;     /* 0=default, 1=full, 2=limited */
    void*   renderer_hint;   /* OakRendererHandle */
    void*   cancelled;       /* CancelAtom* */
} OakDecoderVideoParams;

int oak_decoder_read_video_ex(OakDecoderHandle decoder, int stream_index,
                              const OakDecoderVideoParams* params,
                              OakFrame* out_frame);

/* ------------------------------------------------------------------ */
/*  音频解码 & conform                                                  */
/* ------------------------------------------------------------------ */

int  oak_decoder_read_audio(OakDecoderHandle decoder, int stream_index,
                            int64_t start_sample, int64_t sample_count,
                            float** out_data, int64_t* out_actual_samples);
void oak_audio_buffer_free(float* data);

/* ------------------------------------------------------------------ */
/*  扩展音频解码参数                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    int64_t start_sample;
    int64_t sample_count;
    int     loop_mode;       /* 0=off, 1=loop */
    int     render_mode;     /* 0=offline, 1=online */
    const char* cache_path;  /* NULL = no conform */
} OakDecoderAudioParams;

int oak_decoder_read_audio_ex(OakDecoderHandle decoder, int stream_index,
                              const OakDecoderAudioParams* params,
                              float** out_data, int64_t* out_actual_samples);

/* ------------------------------------------------------------------ */
/*  Conform                                                             */
/* ------------------------------------------------------------------ */

int oak_decoder_conform_audio(OakDecoderHandle decoder,
                              const char* cache_path,
                              int target_sample_rate, int target_channels,
                              OakAudioFormat target_sample_fmt);

int  oak_conform_get(const char* filename, const char* decoder_id,
                     const char* cache_path, int stream_index,
                     int target_sample_rate, int target_channels,
                     OakAudioFormat target_sample_fmt,
                     bool wait,
                     const char*** out_filenames, int* out_count);
int  oak_conform_poll(const char* filename, const char* cache_path,
                      int stream_index,
                      int target_sample_rate, int target_channels,
                      OakAudioFormat target_sample_fmt);
void oak_conform_free_filenames(const char** filenames, int count);

/* ------------------------------------------------------------------ */
/*  编码器                                                              */
/* ------------------------------------------------------------------ */

OakEncoderHandle oak_encoder_create(const char* filepath,
                                    const char* container_format,
                                    const char* video_codec,
                                    const char* audio_codec);
void oak_encoder_close(OakEncoderHandle encoder);

void oak_encoder_set_video_params(OakEncoderHandle encoder,
                                  int width, int height,
                                  OakFramePixelFormat pix_fmt,
                                  int64_t timebase_num, int64_t timebase_den,
                                  double frame_rate);

void oak_encoder_set_video_output_format(OakEncoderHandle encoder,
                                         OakFramePixelFormat output_pix_fmt);

void oak_encoder_set_video_output_colorspace(OakEncoderHandle encoder,
                                             const char* output_colorspace);

void oak_encoder_set_audio_params(OakEncoderHandle encoder,
                                  int sample_rate, int channels,
                                  OakAudioFormat sample_fmt,
                                  int64_t timebase_num, int64_t timebase_den);

int  oak_encoder_write_video(OakEncoderHandle encoder, const OakFrame* frame);
int  oak_encoder_write_audio(OakEncoderHandle encoder,
                             const float* data, int64_t samples,
                             int64_t pts_num, int64_t pts_den);
int  oak_encoder_finalize(OakEncoderHandle encoder);

#ifdef __cplusplus
}
#endif

#endif /* OAK_CODEC_API_H */
