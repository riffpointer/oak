/*
 *  oakcodec.so C API (v2)
 *  Media codec: file open, video/audio/subtitle decode, encode output, conform.
 *  FFmpeg calls are fully encapsulated inside oakcodec.so; external modules must
 *  NEVER directly include or call FFmpeg.
 *
 *  Full-pipeline default: RGBA32F + ACEScg.
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
/*  Stream info (POD)                                                   */
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
/*  Decoder lifecycle                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Open a media file and create a decoder.
 * @param filepath   Absolute or relative path (UTF-8).
 * @param codec_hint Optional codec hint, e.g. "ffmpeg" / "oiio". NULL means auto-detect.
 * @param out_info   Output media info. Caller is responsible for freeing internal arrays
 *                   via oak_media_info_free.
 * @return Decoder handle, or NULL on failure.
 */
OakDecoderHandle oak_decoder_open(const char* filepath, const char* codec_hint,
                                  OakMediaInfo* out_info);

/**
 * @brief Close a decoder and free all associated resources.
 * @param decoder Decoder handle (NULL is silently ignored).
 */
void             oak_decoder_close(OakDecoderHandle decoder);

/**
 * @brief Free the internal arrays inside an OakMediaInfo.
 * @param info Media info pointer (NULL is silently ignored).
 */
void             oak_media_info_free(OakMediaInfo* info);

/* ------------------------------------------------------------------ */
/*  Decoder creation & capability queries                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a decoder by its registered ID (without opening a file).
 * @param id Decoder ID, e.g. "ffmpeg".
 * @return Decoder handle, or NULL if the ID is unknown.
 */
OakDecoderHandle oak_decoder_create_from_id(const char* id);

/**
 * @brief Get the decoder ID string.
 * @param decoder Decoder handle.
 * @return Decoder ID string (constant pointer, do not free).
 */
const char*      oak_decoder_id(OakDecoderHandle decoder);

/** Progress callback: called during indexing / conform. */
typedef void (*OakDecoderProgressCallback)(double progress, void* userdata);

/**
 * @brief Set a progress callback on the decoder.
 * @param decoder Decoder handle.
 * @param cb      Callback function.
 * @param userdata Opaque pointer passed back to the callback.
 */
void oak_decoder_set_progress_callback(OakDecoderHandle decoder,
                                       OakDecoderProgressCallback cb,
                                       void* userdata);

/**
 * @brief Query whether the decoder supports video.
 * @return 1 if yes, 0 if no.
 */
int              oak_decoder_supports_video(OakDecoderHandle decoder);

/**
 * @brief Query whether the decoder supports audio.
 * @return 1 if yes, 0 if no.
 */
int              oak_decoder_supports_audio(OakDecoderHandle decoder);

/**
 * @brief Query whether the decoder is currently open (has an active stream).
 * @return 1 if open, 0 if closed.
 */
int              oak_decoder_is_open(OakDecoderHandle decoder);

/* ------------------------------------------------------------------ */
/*  File probing                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief Probe a file without fully opening it.
 * @param decoder  Decoder handle (used for capability detection).
 * @param filepath File path to probe.
 * @return Allocated OakMediaInfo on success, or NULL on failure.
 *         Caller must free with oak_media_info_free.
 */
OakMediaInfo* oak_decoder_probe_file(OakDecoderHandle decoder, const char* filepath);

/* ------------------------------------------------------------------ */
/*  Stream open                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Open a specific stream inside a file.
 * @param decoder      Decoder handle.
 * @param filepath     File path.
 * @param stream_index Stream index (0-based).
 * @return 0 on success, non-zero on failure.
 */
int oak_decoder_open_stream(OakDecoderHandle decoder, const char* filepath, int stream_index);

/* ------------------------------------------------------------------ */
/*  Video decode (returns OakFrame)                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Read a video frame at the requested time.
 * @param decoder       Decoder handle.
 * @param stream_index  Video stream index (0-based).
 * @param time_num      Target time numerator (in stream timebase).
 * @param time_den      Target time denominator.
 * @param renderer_hint Optional renderer handle (OakRendererHandle cast to void*).
 *                      - NULL: force CPU buffer (codec uses swscale internally).
 *                      - Non-NULL: codec attempts GPU upload; returns OAK_FRAME_GPU or OAK_FRAME_EXTERNAL.
 * @param out_frame     Output frame descriptor. Caller releases via oak_frame_release.
 * @return  0  success
 *          1  need more data (stream not decoded to this point yet; caller should retry later)
 *         -1  error (EOF, decode failure, unsupported format)
 *
 * @note Zero-copy behavior:
 *   1. If the decoder supports hardware acceleration (VideoToolbox / D3D11 / VAAPI / CUDA / MediaCodec)
 *      and renderer_hint is non-NULL, codec returns OAK_FRAME_EXTERNAL without CPU round-trip.
 *   2. If decoder outputs software YUV and renderer_hint is non-NULL:
 *      - codec uploads YUV planes via oak_texture_create_planar;
 *      - by default calls oak_renderer_blit_yuv_to_rgba to convert YUV->RGBA32F on GPU;
 *      - returns OAK_FRAME_GPU with a single RGBA texture.
 *   3. If renderer_hint is NULL, codec falls back to CPU swscale returning OAK_FRAME_CPU.
 *      Target format is fixed to AV_PIX_FMT_RGBAF32LE so the CPU path is also F32.
 *
 * @note Full-pipeline F32 + ACEScg: regardless of GPU or CPU path, the default output
 *       pixel format is RGBA32F and default colorspace is "ACES - ACEScg".
 */
int  oak_decoder_read_video(OakDecoderHandle decoder, int stream_index,
                            int64_t time_num, int64_t time_den,
                            void* renderer_hint,
                            OakFrame* out_frame);

/**
 * @brief Get a thumbnail (quick preview) of a video frame.
 * @param decoder      Decoder handle.
 * @param stream_index Video stream index.
 * @param max_size     Maximum edge length (aspect ratio preserved).
 * @param out_frame    Output frame. Thumbnails are always CPU buffer (OAK_FRAME_CPU).
 * @return 0 on success, non-zero on failure.
 */
int  oak_decoder_thumbnail(OakDecoderHandle decoder, int stream_index,
                           int max_size,
                           OakFrame* out_frame);

/* ------------------------------------------------------------------ */
/*  Extended video decode params                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    int64_t time_num;
    int64_t time_den;
    int     divider;
    int     maximum_format;  /**< OakFramePixelFormat */
    int     force_range;     /**< 0=default, 1=full, 2=limited */
    void*   renderer_hint;   /**< OakRendererHandle */
    void*   cancelled;       /**< CancelAtom* */
} OakDecoderVideoParams;

/**
 * @brief Extended video read with more control parameters.
 * @param decoder Decoder handle.
 * @param stream_index Video stream index.
 * @param params Extended parameters.
 * @param out_frame Output frame.
 * @return 0 on success, non-zero on failure.
 */
int oak_decoder_read_video_ex(OakDecoderHandle decoder, int stream_index,
                              const OakDecoderVideoParams* params,
                              OakFrame* out_frame);

/* ------------------------------------------------------------------ */
/*  Audio decode & conform                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Read audio samples for a given time range (interleaved float32).
 * @param decoder          Decoder handle.
 * @param stream_index     Audio stream index.
 * @param start_sample     Start sample index.
 * @param sample_count     Number of samples to read.
 * @param out_data         Output interleaved float32 buffer. Caller frees via oak_audio_buffer_free.
 * @param out_actual_samples Actual number of samples returned.
 * @return 0 on success, non-zero on failure.
 */
int  oak_decoder_read_audio(OakDecoderHandle decoder, int stream_index,
                            int64_t start_sample, int64_t sample_count,
                            float** out_data, int64_t* out_actual_samples);

/**
 * @brief Free an audio buffer allocated by oak_decoder_read_audio.
 * @param data Buffer pointer.
 */
void oak_audio_buffer_free(float* data);

/* ------------------------------------------------------------------ */
/*  Extended audio decode params                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    int64_t start_sample;
    int64_t sample_count;
    int     loop_mode;       /**< 0=off, 1=loop */
    int     render_mode;     /**< 0=offline, 1=online */
    const char* cache_path;  /**< NULL = no conform */
} OakDecoderAudioParams;

/**
 * @brief Extended audio read with loop / render mode / conform support.
 * @param decoder Decoder handle.
 * @param stream_index Audio stream index.
 * @param params Extended parameters.
 * @param out_data Output buffer.
 * @param out_actual_samples Actual samples returned.
 * @return 0 on success, non-zero on failure.
 */
int oak_decoder_read_audio_ex(OakDecoderHandle decoder, int stream_index,
                              const OakDecoderAudioParams* params,
                              float** out_data, int64_t* out_actual_samples);

/* ------------------------------------------------------------------ */
/*  Conform                                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Conform (pre-process) audio to a target format and cache it.
 * @param decoder            Decoder handle.
 * @param cache_path         Cache directory path.
 * @param target_sample_rate Target sample rate.
 * @param target_channels    Target channel count.
 * @param target_sample_fmt  Target sample format.
 * @return 0 on success, non-zero on failure.
 */
int oak_decoder_conform_audio(OakDecoderHandle decoder,
                              const char* cache_path,
                              int target_sample_rate, int target_channels,
                              OakAudioFormat target_sample_fmt);

typedef void (*OakConformReadyCallback)(void* userdata);

/**
 * @brief Set a global callback notified when conform finishes.
 * @param cb       Callback function.
 * @param userdata Opaque pointer.
 */
void oak_conform_set_ready_callback(OakConformReadyCallback cb, void* userdata);

/**
 * @brief Get or wait for conform results.
 * @param filename         Source filename.
 * @param decoder_id       Decoder ID.
 * @param cache_path       Cache directory.
 * @param stream_index     Stream index.
 * @param target_sample_rate Target sample rate.
 * @param target_channels    Target channels.
 * @param target_sample_fmt  Target format.
 * @param wait             If true, block until conform is ready.
 * @param out_filenames    Output array of cached filenames.
 * @param out_count        Number of cached files.
 * @return 0 on success, non-zero on failure.
 */
int  oak_conform_get(const char* filename, const char* decoder_id,
                     const char* cache_path, int stream_index,
                     int target_sample_rate, int target_channels,
                     OakAudioFormat target_sample_fmt,
                     bool wait,
                     const char*** out_filenames, int* out_count);

/**
 * @brief Poll conform status without blocking.
 * @return 0 if ready, 1 if still processing, -1 on error.
 */
int  oak_conform_poll(const char* filename, const char* cache_path,
                      int stream_index,
                      int target_sample_rate, int target_channels,
                      OakAudioFormat target_sample_fmt);

/**
 * @brief Free an array of conform filenames.
 * @param filenames Array pointer.
 * @param count     Number of entries.
 */
void oak_conform_free_filenames(const char** filenames, int count);

/* ------------------------------------------------------------------ */
/*  Encoder                                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Create an encoder.
 * @param filepath          Output file path.
 * @param container_format  Container format, e.g. "mp4", "mov", "mkv".
 * @param video_codec       Video codec, e.g. "libx264", "prores_ks", "libvpx-vp9".
 * @param audio_codec       Audio codec, e.g. "aac", "libopus", "pcm_s16le".
 * @return Encoder handle, or NULL on failure.
 */
OakEncoderHandle oak_encoder_create(const char* filepath,
                                    const char* container_format,
                                    const char* video_codec,
                                    const char* audio_codec);

/**
 * @brief Close an encoder and finalize the output file.
 * @param encoder Encoder handle (NULL is silently ignored).
 */
void oak_encoder_close(OakEncoderHandle encoder);

/**
 * @brief Set video encoding parameters.
 * @param encoder   Encoder handle.
 * @param width     Video width.
 * @param height    Video height.
 * @param pix_fmt   Pixel format (input expectation, usually RGBA32F).
 * @param timebase_num Timebase numerator.
 * @param timebase_den Timebase denominator.
 * @param frame_rate Frame rate (fps).
 */
void oak_encoder_set_video_params(OakEncoderHandle encoder,
                                  int width, int height,
                                  OakFramePixelFormat pix_fmt,
                                  int64_t timebase_num, int64_t timebase_den,
                                  double frame_rate);

/**
 * @brief Set the target output pixel format for encoding.
 * @param encoder        Encoder handle.
 * @param output_pix_fmt Target pixel format, e.g. OAK_FRAME_PIX_YUV420P8.
 *                       Default is OAK_FRAME_PIX_YUV420P8.
 * @note Encoder input always expects RGBA32F + ACEScg; this only affects
 *       the pre-encode pixel format conversion.
 */
void oak_encoder_set_video_output_format(OakEncoderHandle encoder,
                                         OakFramePixelFormat output_pix_fmt);

/**
 * @brief Set the output colorspace (ODT target).
 * @param encoder         Encoder handle.
 * @param output_colorspace Target colorspace name, e.g. "Output - Rec.709",
 *                          "Output - Rec.2020 PQ", "Output - P3-DCI".
 * @note Encoder first converts ACEScg -> output_colorspace via oakcolor.so,
 *       then converts pixel format before sending to FFmpeg.
 */
void oak_encoder_set_video_output_colorspace(OakEncoderHandle encoder,
                                             const char* output_colorspace);

/**
 * @brief Set audio encoding parameters.
 * @param encoder      Encoder handle.
 * @param sample_rate  Sample rate.
 * @param channels     Channel count.
 * @param sample_fmt   Sample format.
 * @param timebase_num Timebase numerator.
 * @param timebase_den Timebase denominator.
 */
void oak_encoder_set_audio_params(OakEncoderHandle encoder,
                                  int sample_rate, int channels,
                                  OakAudioFormat sample_fmt,
                                  int64_t timebase_num, int64_t timebase_den);

/**
 * @brief Write one video frame.
 * @param encoder Encoder handle.
 * @param frame   Input frame. Can be CPU, GPU, or EXTERNAL.
 *                - Expected format: RGBA32F + "ACES - ACEScg".
 *                - CPU: codec reads data[0] directly.
 *                - GPU/EXTERNAL: codec internally readbacks via oak_renderer_readback_frame.
 * @return 0 on success, non-zero on failure.
 */
int  oak_encoder_write_video(OakEncoderHandle encoder, const OakFrame* frame);

/**
 * @brief Write audio samples.
 * @param encoder Encoder handle.
 * @param data    Interleaved float32 samples.
 * @param samples Number of samples per channel.
 * @param pts_num Presentation timestamp numerator.
 * @param pts_den Presentation timestamp denominator.
 * @return 0 on success, non-zero on failure.
 */
int  oak_encoder_write_audio(OakEncoderHandle encoder,
                             const float* data, int64_t samples,
                             int64_t pts_num, int64_t pts_den);

/**
 * @brief Finalize the encoder and close the output file.
 * @param encoder Encoder handle.
 * @return 0 on success, non-zero on failure.
 */
int  oak_encoder_finalize(OakEncoderHandle encoder);

/* ------------------------------------------------------------------ */
/*  Frame utilities (CPU buffer allocation & pixel format conversion)   */
/*   Used by PluginRenderer and other CPU-path consumers.               */
/* ------------------------------------------------------------------ */

/**
 * @brief Allocate a CPU frame buffer (internally uses AVFrame).
 * @param width      Frame width.
 * @param height     Frame height.
 * @param av_format  FFmpeg AVPixelFormat integer value.
 * @return Internal AVFrame pointer (opaque), or NULL on failure.
 */
void* oak_frame_alloc(int width, int height, int av_format);

/**
 * @brief Free a frame allocated by oak_frame_alloc.
 * @param frame Frame pointer returned by oak_frame_alloc.
 */
void oak_frame_free(void* frame);

/**
 * @brief Get plane data pointer and line stride.
 * @param frame        Frame pointer from oak_frame_alloc.
 * @param plane        Plane index (packed formats use 0).
 * @param out_data     Output data pointer.
 * @param out_linesize Output line stride in bytes.
 * @return 0 on success, -1 on failure.
 */
int oak_frame_get_plane(void* frame, int plane, void** out_data, int* out_linesize);

/**
 * @brief Get frame format parameters.
 * @param frame        Frame pointer.
 * @param out_width    Output width.
 * @param out_height   Output height.
 * @param out_av_format Output AVPixelFormat integer.
 * @return 0 on success, -1 on failure.
 */
int oak_frame_get_params(void* frame, int* out_width, int* out_height, int* out_av_format);

/**
 * @brief Convert frame format using FFmpeg sws_scale.
 * @param src_frame Source frame (from oak_frame_alloc).
 * @param dst_frame Destination frame (pre-allocated).
 * @return 0 on success, -1 on failure.
 */
int oak_frame_convert(void* src_frame, void* dst_frame);

/**
 * @brief Map Olive PixelFormat + channel_count to FFmpeg AVPixelFormat.
 * @param pixel_format   Olive pixel format enum value.
 * @param channel_count  Number of channels (3 for RGB, 4 for RGBA).
 * @return AVPixelFormat integer, or -1 on failure.
 */
int oak_video_format_to_av(int pixel_format, int channel_count);

/**
 * @brief Map FFmpeg AVPixelFormat back to Olive PixelFormat + channel_count.
 * @param av_format            FFmpeg AVPixelFormat integer.
 * @param out_pixel_format     Output Olive pixel format.
 * @param out_channel_count    Output channel count.
 * @return 0 on success, -1 on failure.
 */
int oak_av_to_video_format(int av_format, int* out_pixel_format, int* out_channel_count);

/**
 * @brief Query whether an AVPixelFormat is planar.
 * @param av_format FFmpeg AVPixelFormat integer.
 * @return 1 if planar, 0 if packed, -1 if unknown.
 */
int oak_video_format_is_planar(int av_format);

/**
 * @brief Get a compatible pixel format (fallback to available format).
 * @param pixel_format Olive pixel format.
 * @return Compatible pixel format integer.
 */
int oak_video_format_compatible(int pixel_format);

#ifdef __cplusplus
}
#endif

#endif /* OAK_CODEC_API_H */
