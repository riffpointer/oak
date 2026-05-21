# oakcodec.so C API 设计

> 媒体编解码：文件打开、视频/音频/字幕解码、编码输出、conform（音频预处理）。
> 内部使用 FFmpeg + OpenImageIO，对外完全隐藏。

## 一、类型定义

```c
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakDecoder* OakDecoderHandle;
typedef struct OakEncoder* OakEncoderHandle;
typedef struct OakConform* OakConformHandle;

/**
 * @brief 像素格式枚举。
 */
typedef enum {
    OAK_PIX_FMT_INVALID = 0,
    OAK_PIX_FMT_RGBA8,       // 8-bit unsigned per channel
    OAK_PIX_FMT_RGBA16,      // 16-bit unsigned per channel
    OAK_PIX_FMT_RGBA32F,     // 32-bit float per channel
    OAK_PIX_FMT_RGB8,
    OAK_PIX_FMT_YUV420P8,
    OAK_PIX_FMT_YUV422P8,
    OAK_PIX_FMT_YUV444P8,
} OakPixelFormat;

/**
 * @brief 音频采样格式。
 */
typedef enum {
    OAK_AUDIO_FMT_INVALID = 0,
    OAK_AUDIO_FMT_U8,
    OAK_AUDIO_FMT_S16,
    OAK_AUDIO_FMT_S32,
    OAK_AUDIO_FMT_FLT,
    OAK_AUDIO_FMT_DBL,
} OakAudioFormat;

/**
 * @brief 视频流参数（POD）。
 */
typedef struct {
    int width;
    int height;
    OakPixelFormat pix_fmt;
    int64_t timebase_num;   // 时间基分子
    int64_t timebase_den;   // 时间基分母
    double  frame_rate;
    int64_t duration_frames;
} OakVideoStreamInfo;

/**
 * @brief 音频流参数（POD）。
 */
typedef struct {
    int sample_rate;
    int channels;
    OakAudioFormat sample_fmt;
    int64_t timebase_num;
    int64_t timebase_den;
    int64_t duration_samples;
} OakAudioStreamInfo;

/**
 * @brief 媒体文件信息。
 */
typedef struct {
    int video_stream_count;
    int audio_stream_count;
    int subtitle_stream_count;
    OakVideoStreamInfo* video_streams;   // 数组，长度 video_stream_count
    OakAudioStreamInfo* audio_streams;   // 数组，长度 audio_stream_count
} OakMediaInfo;
```

## 二、解码器生命周期

```c
/**
 * @brief 打开媒体文件并创建解码器。
 * @param filepath 绝对或相对路径（UTF-8）。
 * @param codec_hint 可选的编解码器提示字符串（如 "ffmpeg"），NULL 表示自动探测。
 * @param out_info 输出媒体信息。调用者负责用 oak_media_info_free 释放内部数组。
 * @return 解码器句柄，NULL 表示打开失败。
 */
OakDecoderHandle oak_decoder_open(const char* filepath, const char* codec_hint,
                                  OakMediaInfo* out_info);

/**
 * @brief 关闭解码器并释放所有资源。
 * @param decoder 解码器句柄。传 NULL 是安全的。
 */
void oak_decoder_close(OakDecoderHandle decoder);

/**
 * @brief 释放 oak_decoder_open 返回的 OakMediaInfo 内部数组。
 * @param info 媒体信息结构体。
 */
void oak_media_info_free(OakMediaInfo* info);
```

## 三、视频解码

```c
/**
 * @brief 读取指定时间点的视频帧。
 * @param decoder 解码器句柄。
 * @param stream_index 视频流索引（0-based）。
 * @param time_num 目标时间分子（以流的时间基为单位）。
 * @param time_den 目标时间分母。
 * @param out_pix_fmt 请求的输出像素格式。
 * @param out_width 请求的输出宽度（若与源不同则自动缩放）。
 * @param out_height 请求的输出高度。
 * @param out_data 输出图像数据指针。由 oakcodec.so 内部分配，调用者必须通过 oak_frame_free 释放。
 * @param out_stride 输出每行字节数。
 * @return 0 成功，1 需要更多数据（流未解码到该位置），-1 错误（EOF 或解码失败）。
 * @note 若 out_pix_fmt / out_width / out_height 与源不同，内部会自动做格式转换和缩放。
 */
int oak_decoder_read_video(OakDecoderHandle decoder, int stream_index,
                           int64_t time_num, int64_t time_den,
                           OakPixelFormat out_pix_fmt,
                           int out_width, int out_height,
                           void** out_data, int* out_stride);

/**
 * @brief 释放视频帧数据。
 * @param data 由 oak_decoder_read_video 分配的指针。
 */
void oak_frame_free(void* data);

/**
 * @brief 获取视频帧的缩略图（快速预览）。
 * @param max_size 缩略图最大边长（保持宽高比）。
 * @return 0 成功，非 0 失败。
 */
int oak_decoder_thumbnail(OakDecoderHandle decoder, int stream_index,
                          int max_size,
                          void** out_data, int* out_width, int* out_height,
                          int* out_stride);
```

## 四、音频解码与 Conform

```c
/**
 * @brief 读取指定时间范围的音频采样（交错 float32）。
 * @param decoder 解码器句柄。
 * @param stream_index 音频流索引。
 * @param start_sample 起始采样索引。
 * @param sample_count 请求的采样数。
 * @param out_data 输出 PCM 数据指针。由 oakcodec.so 分配，调用者通过 oak_audio_buffer_free 释放。
 * @param out_actual_samples 实际读取的采样数（可能少于请求，如遇到 EOF）。
 * @return 0 成功，非 0 失败。
 * @note 输出格式统一为交错 float32，sample_rate 和 channels 与请求一致。
 *       若源格式不同，内部会自动重采样和格式转换。
 */
int oak_decoder_read_audio(OakDecoderHandle decoder, int stream_index,
                           int64_t start_sample, int64_t sample_count,
                           float** out_data, int64_t* out_actual_samples);

void oak_audio_buffer_free(float* data);

/* ---- Conform（音频预处理缓存）---- */

/**
 * @brief 查询或创建 conform 任务。
 * @param decoder_id 解码器唯一标识（通常由 oakcore.so 的 Footage 节点提供）。
 * @param cache_path 缓存目录路径。
 * @param stream_index 音频流索引。
 * @param target_sample_rate 目标采样率。
 * @param target_channels 目标通道数。
 * @param wait 若为 true，则阻塞直到 conform 完成并返回帧；若为 false，则立即返回状态。
 * @param out_filenames 输出 conform 后的文件路径数组（每个通道一个文件）。
 * @param out_count 输出数组长度。
 * @return 状态：0 = 已存在可直接使用，1 = 正在生成（需稍后重试），-1 = 错误。
 * @note 若返回 1，调用者可通过 oak_conform_poll 轮询状态。
 */
int oak_conform_get(const char* decoder_id, const char* cache_path,
                    int stream_index,
                    int target_sample_rate, int target_channels,
                    bool wait,
                    const char*** out_filenames, int* out_count);

/**
 * @brief 轮询 conform 任务状态。
 * @param decoder_id 解码器标识。
 * @return 0 = 完成，1 = 进行中，-1 = 失败。
 */
int oak_conform_poll(const char* decoder_id);

/**
 * @brief 释放 oak_conform_get 返回的文件名数组。
 */
void oak_conform_free_filenames(const char** filenames, int count);
```

## 五、编码器

```c
/**
 * @brief 创建编码器。
 * @param filepath 输出文件路径（UTF-8）。
 * @param container_format 容器格式，如 "mp4"、"mov"、"mkv"。
 * @param video_codec 视频编码器，如 "h264"、"prores"、"png"（序列帧）。NULL 表示无视频轨道。
 * @param audio_codec 音频编码器，如 "aac"、"pcm_s16le"。NULL 表示无音频轨道。
 * @return 编码器句柄，NULL 表示失败。
 */
OakEncoderHandle oak_encoder_create(const char* filepath,
                                    const char* container_format,
                                    const char* video_codec,
                                    const char* audio_codec);

void oak_encoder_close(OakEncoderHandle encoder);

/**
 * @brief 配置视频轨道参数。必须在写入第一帧前调用。
 */
void oak_encoder_set_video_params(OakEncoderHandle encoder,
                                  int width, int height, OakPixelFormat pix_fmt,
                                  int64_t timebase_num, int64_t timebase_den,
                                  double frame_rate);

/**
 * @brief 配置音频轨道参数。必须在写入第一帧前调用。
 */
void oak_encoder_set_audio_params(OakEncoderHandle encoder,
                                  int sample_rate, int channels,
                                  OakAudioFormat sample_fmt,
                                  int64_t timebase_num, int64_t timebase_den);

/**
 * @brief 写入一帧视频。
 * @param data 图像数据指针。
 * @param stride 每行字节数。
 * @param pts 显示时间戳（以编码器时间基为单位）。
 * @return 0 成功，非 0 失败。
 */
int oak_encoder_write_video(OakEncoderHandle encoder,
                            const void* data, int stride,
                            int64_t pts_num, int64_t pts_den);

/**
 * @brief 写入一段音频。
 * @param data 交错 float32 PCM 数据。
 * @param samples 采样数（每通道）。
 * @param pts 时间戳。
 */
int oak_encoder_write_audio(OakEncoderHandle encoder,
                            const float* data, int64_t samples,
                            int64_t pts_num, int64_t pts_den);

/**
 * @brief 完成编码并写入文件尾。
 * @return 0 成功，非 0 失败。
 */
int oak_encoder_finalize(OakEncoderHandle encoder);
```

#ifdef __cplusplus
}
#endif
