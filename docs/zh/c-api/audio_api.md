# oakaudio.so C API 设计

> 音频处理：混合、重采样、格式转换、效果链。
> 内部可用任意音频库（现阶段可用 Qt + FFmpeg 的 swresample），对外隐藏。

## 一、类型定义

```c
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakAudioMixer*    OakAudioMixerHandle;
typedef struct OakAudioResampler* OakAudioResamplerHandle;
typedef struct OakAudioBuffer*   OakAudioBufferHandle;

/**
 * @brief 音频参数（POD）。
 */
typedef struct {
    int sample_rate;
    int channels;
    int64_t duration_samples;
} OakAudioParams;
```

## 二、音频缓冲区

```c
/**
 * @brief 创建音频缓冲区。
 * @param channels 通道数。
 * @param samples 每通道采样数。
 * @param sample_rate 采样率。
 * @param interleaved 是否为交错格式（true = 交错 float32，false = planar float32）。
 * @return 缓冲区句柄，NULL 表示内存不足。
 */
OakAudioBufferHandle oak_audio_buffer_create(int channels, int64_t samples,
                                             int sample_rate, bool interleaved);

void oak_audio_buffer_free(OakAudioBufferHandle buf);

/**
 * @brief 获取缓冲区参数。
 */
void oak_audio_buffer_params(OakAudioBufferHandle buf, OakAudioParams* out_params);

/**
 * @brief 获取数据指针。
 * @param out_data 输出数据指针。交错格式下指向所有通道交错的 float32 数组；
 *                 planar 格式下指向 float* 数组，每个元素是一个通道的指针。
 * @param out_interleaved 输出是否为交错格式。
 */
void oak_audio_buffer_data(OakAudioBufferHandle buf, void** out_data, bool* out_interleaved);

/**
 * @brief 深拷贝缓冲区。
 */
OakAudioBufferHandle oak_audio_buffer_clone(OakAudioBufferHandle buf);
```

## 三、重采样器

```c
/**
 * @brief 创建重采样器。
 * @param src_channels 源通道数。
 * @param src_sample_rate 源采样率。
 * @param dst_channels 目标通道数。
 * @param dst_sample_rate 目标采样率。
 * @return 重采样器句柄。
 */
OakAudioResamplerHandle oak_audio_resampler_create(int src_channels, int src_sample_rate,
                                                   int dst_channels, int dst_sample_rate);

void oak_audio_resampler_free(OakAudioResamplerHandle resampler);

/**
 * @brief 重采样一段音频。
 * @param resampler 重采样器句柄。
 * @param in_data 输入数据指针（交错 float32）。
 * @param in_samples 输入采样数。
 * @param out_data 输出数据指针（调用者分配）。
 * @param out_samples_capacity out_data 的最大容量（采样数）。
 * @param out_actual_samples 实际输出的采样数。
 * @return 0 成功，1 输出容量不足（需扩大缓冲区后重试），-1 错误。
 */
int oak_audio_resampler_process(OakAudioResamplerHandle resampler,
                                const float* in_data, int64_t in_samples,
                                float* out_data, int64_t out_samples_capacity,
                                int64_t* out_actual_samples);
```

## 四、混音器

```c
/**
 * @brief 创建混音器。
 * @param channels 输出通道数。
 * @param sample_rate 输出采样率。
 * @return 混音器句柄。
 */
OakAudioMixerHandle oak_audio_mixer_create(int channels, int sample_rate);

void oak_audio_mixer_free(OakAudioMixerHandle mixer);

/**
 * @brief 向混音器添加一个源缓冲区。
 * @param mixer 混音器句柄。
 * @param buf 音频缓冲区句柄。混音器内部会增加引用计数，不接管所有权。
 * @param start_sample 该源在输出时间线上的起始采样位置（可为负，表示从中间开始）。
 * @param volume 音量增益（线性，1.0 = 原音量）。
 * @param pan 声像（-1.0 = 左，0.0 = 中，1.0 = 右）。对于单声道源有效。
 */
void oak_audio_mixer_add_source(OakAudioMixerHandle mixer,
                                OakAudioBufferHandle buf,
                                int64_t start_sample,
                                double volume,
                                double pan);

/**
 * @brief 清空所有源。
 */
void oak_audio_mixer_clear_sources(OakAudioMixerHandle mixer);

/**
 * @brief 混合并输出指定范围的采样。
 * @param mixer 混音器句柄。
 * @param start_sample 输出起始采样位置。
 * @param sample_count 请求采样的数量。
 * @param out_data 输出缓冲区（交错 float32，调用者分配，大小 >= channels * sample_count * sizeof(float)）。
 * @return 0 成功，非 0 失败。
 * @note 若某个源在指定范围内无数据，则该位置静默（0.0f）。
 */
int oak_audio_mixer_mix(OakAudioMixerHandle mixer,
                        int64_t start_sample, int64_t sample_count,
                        float* out_data);
```

## 五、简单工具函数

```c
/**
 * @brief 将一种布局的音频数据转换为另一种布局。
 * @param in_data 输入数据。
 * @param in_channels 输入通道数。
 * @param in_samples 输入采样数。
 * @param in_interleaved 输入是否为交错格式。
 * @param out_data 输出数据（调用者分配）。
 * @param out_channels 输出通道数。
 * @param out_interleaved 输出是否为交错格式。
 * @return 0 成功，非 0 失败。
 * @note 若通道数不同，会自动做 upmix/downmix（如立体声转单声道取平均）。
 */
int oak_audio_convert_layout(const float* in_data, int in_channels, int64_t in_samples, bool in_interleaved,
                             float* out_data, int out_channels, int64_t out_samples, bool out_interleaved);

#ifdef __cplusplus
}
#endif
```
