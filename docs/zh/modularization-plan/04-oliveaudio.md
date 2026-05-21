# liboliveaudio.so — 音频播放与处理

> **依赖**：`libolivecore.so`  
> **外部依赖**：PortAudio，Qt::Core  
> **包含源码**：`app/audio/`  
> **当前状态**：单体 OBJECT 库的一部分  
> **改造难度**：⭐⭐（较简单）

---

## 1. 当前状态分析

`app/audio/` 负责音频播放管理、音频处理管线和波形可视化数据。

| 组件 | 说明 |
|---|---|
| `audiomanager.h/cpp` | 音频播放管理器（单例），基于 PortAudio |
| `audioprocessor.h/cpp` | 音频处理管线 |
| `audiovisualwaveform.h/cpp` | 音频波形数据（用于 UI 显示） |
| `audiohybriddevice.h/cpp` | 音频混合设备 |

**特点**：
- 相对独立，不直接依赖 `node/` 或 `render/`（通过回调或数据缓冲区交互）。
- `AudioManager` 是单例，C API 中需要妥善处理单例的生命周期。
- 音频数据量较小，实时性要求高。

---

## 2. C API 设计

### 2.1 头文件：`c_api/include/olive/audio_api.h`

```c
#ifndef OLIVE_AUDIO_API_H
#define OLIVE_AUDIO_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core_api.h"

#define OLIVE_AUDIO_API_VERSION 1

#ifdef OLIVE_BUILDING_AUDIO
#  define OLIVE_AUDIO_API __attribute__((visibility("default")))
#else
#  define OLIVE_AUDIO_API
#endif

/* ========== 类型前向声明 ========== */
typedef struct OliveAudioManager     OliveAudioManager;
typedef struct OliveAudioProcessor   OliveAudioProcessor;
typedef struct OliveAudioWaveform    OliveAudioWaveform;

/* ========== API 版本 ========== */
OLIVE_AUDIO_API int olive_audio_api_version(void);

/* ========== AudioManager（播放控制） ========== */
OLIVE_AUDIO_API OliveAudioManager* olive_audio_manager_get_instance(void);
OLIVE_AUDIO_API void olive_audio_manager_release_instance(OliveAudioManager* mgr);

OLIVE_AUDIO_API int olive_audio_manager_init(OliveAudioManager* mgr, OliveAudioParams params);
OLIVE_AUDIO_API void olive_audio_manager_shutdown(OliveAudioManager* mgr);

// 播放控制
OLIVE_AUDIO_API int olive_audio_manager_play(OliveAudioManager* mgr);
OLIVE_AUDIO_API int olive_audio_manager_pause(OliveAudioManager* mgr);
OLIVE_AUDIO_API int olive_audio_manager_stop(OliveAudioManager* mgr);
OLIVE_AUDIO_API int olive_audio_manager_is_playing(OliveAudioManager* mgr);

// 推入待播放的音频缓冲区（主进程渲染后推入）
OLIVE_AUDIO_API int olive_audio_manager_push_buffer(OliveAudioManager* mgr,
                                                       OliveSampleBuffer* buffer);

// 获取当前播放时间
OLIVE_AUDIO_API OliveRational olive_audio_manager_get_playback_time(OliveAudioManager* mgr);

/* ========== AudioProcessor（处理管线） ========== */
OLIVE_AUDIO_API OliveAudioProcessor* olive_audio_processor_create(OliveAudioParams params);
OLIVE_AUDIO_API void olive_audio_processor_destroy(OliveAudioProcessor* proc);

// 处理一帧音频（应用音量、声像等）
OLIVE_AUDIO_API int olive_audio_processor_process(OliveAudioProcessor* proc,
                                                     OliveSampleBuffer* input,
                                                     OliveSampleBuffer** output);

/* ========== AudioWaveform（波形数据） ========== */
OLIVE_AUDIO_API OliveAudioWaveform* olive_audio_waveform_create(OliveAudioParams params,
                                                                 OliveRational duration);
OLIVE_AUDIO_API void olive_audio_waveform_destroy(OliveAudioWaveform* wf);

// 从采样缓冲区生成波形数据
OLIVE_AUDIO_API int olive_audio_waveform_generate(OliveAudioWaveform* wf,
                                                   OliveSampleBuffer* buffer,
                                                   OliveRational start_time);

// 获取指定时间点的波形峰值（用于 UI 绘制）
OLIVE_AUDIO_API float olive_audio_waveform_get_peak(OliveAudioWaveform* wf,
                                                      OliveRational time,
                                                      int channel);

// 获取波形数据数组（用于批量绘制）
OLIVE_AUDIO_API const float* olive_audio_waveform_get_peaks(OliveAudioWaveform* wf,
                                                               int channel,
                                                               int* out_count);

#ifdef __cplusplus
}
#endif

#endif  // OLIVE_AUDIO_API_H
```

### 2.2 实现要点

```cpp
// c_api/src/audio_api.cpp

#include "olive/audio_api.h"
#include "audio/audiomanager.h"
#include "audio/audioprocessor.h"
#include "audio/audiovisualwaveform.h"

struct OliveAudioManager {
    // AudioManager 是单例，此处不持有所有权，只作为句柄
    olive::AudioManager* impl;
};

extern "C" {

OliveAudioManager* olive_audio_manager_get_instance(void) {
    static OliveAudioManager mgr;
    mgr.impl = olive::AudioManager::instance();
    return &mgr;
}

void olive_audio_manager_release_instance(OliveAudioManager* mgr) {
    // 单例不在这里销毁
    (void)mgr;
}

int olive_audio_manager_init(OliveAudioManager* mgr, OliveAudioParams params) {
    if (!mgr || !mgr->impl) return OLIVE_ERROR_INVALID;
    try {
        olive::AudioParams cpp_params;
        cpp_params.set_sample_rate(params.sample_rate);
        cpp_params.set_channel_layout(params.channel_layout);
        // ... 转换 format ...
        mgr->impl->SetParameters(cpp_params);
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

int olive_audio_manager_push_buffer(OliveAudioManager* mgr, OliveSampleBuffer* buffer) {
    if (!mgr || !mgr->impl || !buffer) return OLIVE_ERROR_INVALID;
    try {
        mgr->impl->PushBuffer(buffer->impl);
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

// ... 其他函数类似封装 ...

}  // extern "C"
```

---

## 3. CMake 改造

```cmake
# app/audio/CMakeLists.txt

set(AUDIO_INTERNAL_SOURCES
  audiomanager.cpp audiomanager.h
  audioprocessor.cpp audioprocessor.h
  audiovisualwaveform.cpp audiovisualwaveform.h
  audiohybriddevice.cpp audiohybriddevice.h
)

set(AUDIO_API_SOURCES
  ${CMAKE_SOURCE_DIR}/c_api/src/audio_api.cpp
)

add_library(oliveaudio SHARED
  ${AUDIO_INTERNAL_SOURCES}
  ${AUDIO_API_SOURCES}
)

target_compile_definitions(oliveaudio PRIVATE OLIVE_BUILDING_AUDIO)

target_include_directories(oliveaudio
  PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/c_api/include
  PUBLIC
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(oliveaudio
  PUBLIC
    olivecore
    ${PORTAUDIO_LIBRARIES}
    Qt${QT_VERSION_MAJOR}::Core
)

set_target_properties(oliveaudio PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

install(TARGETS oliveaudio DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/c_api/include/olive/audio_api.h DESTINATION include/olive)
```

---

## 4. 小步快跑实施步骤

### Step 0: 分析音频数据流（半天）

- [ ] 梳理 `AudioManager` 的数据流：谁调用 `PushBuffer`？谁消费？
- [ ] 确认 `audio/` 对 `node/` 的依赖情况（`AudioProcessor` 是否直接操作 `Node`？）。

**验收标准**：确认 `audio/` 可以独立于 `node/` 编译（可能只需要 `SampleBuffer` 和 `AudioParams` 类型）。

### Step 1: 独立编译 liboliveaudio.so（1 天）

- [ ] 将 `app/audio/` 从主 OBJECT 库移出，单独构建为 `oliveaudio SHARED`。
- [ ] 确保 `AudioManager` 单例的初始化顺序正确（Qt 的 `Q_GLOBAL_STATIC` 或延迟初始化）。

**验收标准**：`liboliveaudio.so` 编译成功。

### Step 2: 最小 C API（1 天）

- [ ] 实现播放控制：`init`, `play`, `pause`, `stop`。
- [ ] 实现 `push_buffer`（关键接口，主进程渲染音频后推入播放队列）。

**验收标准**：可以通过 C API 初始化音频、播放一段静音缓冲区。

### Step 3: 波形数据接口（1 天）

- [ ] 实现 `olive_audio_waveform_create/generate/get_peak`。
- [ ] 此接口供 UI 层调用以绘制音频波形。

**验收标准**：给定一个 `OliveSampleBuffer*`，可以生成波形并查询任意时间点的峰值。

### Step 4: 处理器接口（1 天）

- [ ] 实现 `olive_audio_processor_create/process`。
- [ ] 用于节点图中的音频处理链（音量、声像）。

---

## 5. 风险与回退

| 风险 | 对策 |
|---|---|
| `AudioManager` 单例在动态库卸载后仍被引用 | 主进程退出前先停止播放并释放 `AudioManager`，再卸载动态库。 |
| PortAudio 回调线程与主线程的交互 | C API 中 `push_buffer` 是线程安全的（内部用 `QMutex` 保护队列），C API 调用者无需额外同步。 |
| 实时音频延迟要求 | C API 不增加额外拷贝：`push_buffer` 内部直接传递 `SampleBuffer` 的共享指针。 |
| 音频处理需要节点图信息 | `AudioProcessor` 的参数（如音量值）通过 C API 直接设置，不涉及节点图遍历。节点图到音频参数的映射在 `libolivenode.so` 中完成。 |
