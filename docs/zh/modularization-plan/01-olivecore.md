# libolivecore.so — 基础数据类型库

> **依赖**：无（不依赖其他 Olive 模块）  
> **外部依赖**：FFmpeg::avutil, OpenGL::GL, Imath::Imath  
> **当前状态**：`ext/core/` 目录，已作为独立静态库编译  
> **改造难度**：⭐（最简单）

---

## 1. 当前状态分析

`ext/core/` 是项目中最为独立的模块，包含纯数据类型和数学工具：

| 文件/类 | 职责 |
|---|---|
| `rational.h` | 有理数（帧率、时间） |
| `color.h` | 颜色表示与运算 |
| `timecodefunctions.h` | 时间码格式化 |
| `timerange.h` | 时间范围 `[in, out)` |
| `samplebuffer.h` | 音频采样缓冲区 |
| `pixelformat.h` | 像素格式枚举 |
| `videoparams.h` / `audioparams.h` | 视频/音频参数 |
| `bezier.h` / `math.h` | 数学工具 |
| `stringutils.h` / `value.h` | 字符串与通用值 |

**优势**：
- 无 Qt GUI 依赖（仅用 `Qt::Core` 的基础类型）。
- 无项目内其他模块依赖。
- 主要是 POD（Plain Old Data）类型和纯函数。

---

## 2. C API 设计

### 2.1 头文件：`c_api/include/olive/core_api.h`

```c
#ifndef OLIVE_CORE_API_H
#define OLIVE_CORE_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define OLIVE_CORE_API_VERSION 1

/* ========== 导出宏 ========== */
#ifdef OLIVE_BUILDING_CORE
#  define OLIVE_CORE_API __attribute__((visibility("default")))
#else
#  define OLIVE_CORE_API
#endif

/* ========== 枚举 ========== */

typedef enum {
    OLIVE_PIXEL_FMT_INVALID = 0,
    OLIVE_PIXEL_FMT_RGBA8,
    OLIVE_PIXEL_FMT_RGBA16,
    OLIVE_PIXEL_FMT_RGBA32F,
    OLIVE_PIXEL_FMT_RGB8,
    OLIVE_PIXEL_FMT_YUV420P,
    OLIVE_PIXEL_FMT_YUV422P,
    OLIVE_PIXEL_FMT_YUV444P,
    OLIVE_PIXEL_FMT_COUNT
} OlivePixelFormat;

typedef enum {
    OLIVE_SAMPLE_FMT_INVALID = 0,
    OLIVE_SAMPLE_FMT_U8,
    OLIVE_SAMPLE_FMT_S16,
    OLIVE_SAMPLE_FMT_S32,
    OLIVE_SAMPLE_FMT_FLT,
    OLIVE_SAMPLE_FMT_DBL,
    OLIVE_SAMPLE_FMT_U8P,
    OLIVE_SAMPLE_FMT_S16P,
    OLIVE_SAMPLE_FMT_S32P,
    OLIVE_SAMPLE_FMT_FLTP,
    OLIVE_SAMPLE_FMT_DBLP,
    OLIVE_SAMPLE_FMT_COUNT
} OliveSampleFormat;

typedef enum {
    OLIVE_OK = 0,
    OLIVE_ERROR_GENERIC = -1,
    OLIVE_ERROR_INVALID = -2,
    OLIVE_ERROR_NOMEM = -3,
    OLIVE_ERROR_NOT_FOUND = -4,
    OLIVE_ERROR_IO = -5,
    OLIVE_ERROR_CANCELLED = -6,
    OLIVE_ERROR_UNSUPPORTED = -7,
} OliveResult;

/* ========== POD 结构体 ========== */

typedef struct {
    int64_t num;
    int64_t den;
} OliveRational;

typedef struct {
    double r;
    double g;
    double b;
    double a;
} OliveColor;

typedef struct {
    int width;
    int height;
} OliveSize;

typedef struct {
    int width;
    int height;
    int depth;
    int channel_count;
    OlivePixelFormat format;
    double pixel_aspect_num;
    double pixel_aspect_den;
} OliveVideoParams;

typedef struct {
    int sample_rate;
    int64_t channel_layout;  // FFmpeg AV_CH_LAYOUT_* 值
    OliveSampleFormat format;
} OliveAudioParams;

/* ========== API 版本 ========== */
OLIVE_CORE_API int olive_core_api_version(void);

/* ========== 内存管理 ========== */
OLIVE_CORE_API void olive_core_free(void* ptr);
OLIVE_CORE_API void* olive_core_alloc(size_t size);
OLIVE_CORE_API void* olive_core_realloc(void* ptr, size_t size);

/* ========== 错误处理 ========== */
OLIVE_CORE_API int olive_core_last_error_code(void);
OLIVE_CORE_API const char* olive_core_last_error_string(void);

/* ========== Rational ========== */
OLIVE_CORE_API OliveRational olive_rational_make(int64_t num, int64_t den);
OLIVE_CORE_API OliveRational olive_rational_add(OliveRational a, OliveRational b);
OLIVE_CORE_API OliveRational olive_rational_sub(OliveRational a, OliveRational b);
OLIVE_CORE_API OliveRational olive_rational_mul(OliveRational a, OliveRational b);
OLIVE_CORE_API OliveRational olive_rational_div(OliveRational a, OliveRational b);
OLIVE_CORE_API double olive_rational_to_double(OliveRational r);
OLIVE_CORE_API OliveRational olive_rational_from_double(double v, int64_t max_den);
OLIVE_CORE_API int olive_rational_cmp(OliveRational a, OliveRational b);
OLIVE_CORE_API int olive_rational_is_valid(OliveRational r);
OLIVE_CORE_API void olive_rational_reduce(OliveRational* r);

/* ========== Color ========== */
OLIVE_CORE_API OliveColor olive_color_make(double r, double g, double b, double a);
OLIVE_CORE_API OliveColor olive_color_add(OliveColor a, OliveColor b);
OLIVE_CORE_API OliveColor olive_color_mul_scalar(OliveColor c, double s);

/* ========== TimeRange ========== */
typedef struct OliveTimeRange OliveTimeRange;

OLIVE_CORE_API OliveTimeRange* olive_time_range_create(OliveRational in, OliveRational out);
OLIVE_CORE_API void olive_time_range_destroy(OliveTimeRange* tr);
OLIVE_CORE_API OliveRational olive_time_range_in(OliveTimeRange* tr);
OLIVE_CORE_API OliveRational olive_time_range_out(OliveTimeRange* tr);
OLIVE_CORE_API OliveRational olive_time_range_length(OliveTimeRange* tr);
OLIVE_CORE_API int olive_time_range_contains(OliveTimeRange* tr, OliveRational t);
OLIVE_CORE_API int olive_time_range_overlaps(OliveTimeRange* a, OliveTimeRange* b);

/* ========== Timecode ========== */
OLIVE_CORE_API char* olive_timecode_from_rational(OliveRational time,
                                                   OliveRational timebase,
                                                   int display_mode);
OLIVE_CORE_API OliveRational olive_timecode_to_rational(const char* timecode,
                                                         OliveRational timebase);

/* ========== PixelFormat ========== */
OLIVE_CORE_API int olive_pixel_format_bytes_per_channel(OlivePixelFormat fmt);
OLIVE_CORE_API int olive_pixel_format_channel_count(OlivePixelFormat fmt);
OLIVE_CORE_API size_t olive_pixel_format_frame_size(OlivePixelFormat fmt, int width, int height);
OLIVE_CORE_API const char* olive_pixel_format_name(OlivePixelFormat fmt);

/* ========== SampleBuffer ========== */
typedef struct OliveSampleBuffer OliveSampleBuffer;

OLIVE_CORE_API OliveSampleBuffer* olive_sample_buffer_create(OliveAudioParams params,
                                                              int sample_count);
OLIVE_CORE_API void olive_sample_buffer_destroy(OliveSampleBuffer* buf);
OLIVE_CORE_API int olive_sample_buffer_sample_count(OliveSampleBuffer* buf);
OLIVE_CORE_API int olive_sample_buffer_channel_count(OliveSampleBuffer* buf);
OLIVE_CORE_API void* olive_sample_buffer_channel_data(OliveSampleBuffer* buf, int channel);
OLIVE_CORE_API size_t olive_sample_buffer_channel_data_size(OliveSampleBuffer* buf);
OLIVE_CORE_API OliveAudioParams olive_sample_buffer_params(OliveSampleBuffer* buf);
OLIVE_CORE_API OliveSampleBuffer* olive_sample_buffer_silence(OliveAudioParams params,
                                                               int sample_count);

/* ========== VideoParams 辅助 ========== */
OLIVE_CORE_API size_t olive_video_params_frame_size(OliveVideoParams params);
OLIVE_CORE_API int olive_video_params_is_valid(OliveVideoParams params);

#ifdef __cplusplus
}
#endif

#endif  // OLIVE_CORE_API_H
```

### 2.2 实现：`c_api/src/core_api.cpp`

```cpp
#include "olive/core_api.h"
#include <olive/core/core.h>
#include <olive/core/rational.h>
#include <olive/core/color.h>
#include <olive/core/timerange.h>
#include <olive/core/samplebuffer.h>
#include <olive/core/pixelformat.h>
#include <olive/core/timecodefunctions.h>
#include <cstring>
#include <cstdlib>

// 线程局部错误状态
thread_local int g_last_error_code = OLIVE_OK;
thread_local char g_last_error_string[1024];

static void SetError(int code, const char* msg) {
    g_last_error_code = code;
    strncpy(g_last_error_string, msg, sizeof(g_last_error_string) - 1);
    g_last_error_string[sizeof(g_last_error_string) - 1] = '\0';
}

extern "C" {

int olive_core_api_version(void) { return OLIVE_CORE_API_VERSION; }

void olive_core_free(void* ptr) { free(ptr); }
void* olive_core_alloc(size_t size) { return malloc(size); }
void* olive_core_realloc(void* ptr, size_t size) { return realloc(ptr, size); }

int olive_core_last_error_code(void) { return g_last_error_code; }
const char* olive_core_last_error_string(void) { return g_last_error_string; }

OliveRational olive_rational_make(int64_t num, int64_t den) {
    return {num, den};
}

OliveRational olive_rational_add(OliveRational a, OliveRational b) {
    olive::Rational ra(a.num, a.den);
    olive::Rational rb(b.num, b.den);
    auto rc = ra + rb;
    return {rc.numerator(), rc.denominator()};
}

// ... 其他 rational 运算类似封装 ...

double olive_rational_to_double(OliveRational r) {
    return olive::Rational(r.num, r.den).toDouble();
}

OliveColor olive_color_make(double r, double g, double b, double a) {
    return {r, g, b, a};
}

OliveTimeRange* olive_time_range_create(OliveRational in, OliveRational out) {
    try {
        auto* tr = new OliveTimeRange();
        // 内部持有 olive::TimeRange 指针
        // tr->impl = new olive::TimeRange(...);
        return tr;
    } catch (...) {
        SetError(OLIVE_ERROR_NOMEM, "Failed to create TimeRange");
        return nullptr;
    }
}

void olive_time_range_destroy(OliveTimeRange* tr) {
    if (tr) {
        delete tr;
    }
}

OliveRational olive_time_range_in(OliveTimeRange* tr) {
    // auto r = tr->impl->in();
    // return {r.numerator(), r.denominator()};
    return {0, 1};  // 占位
}

// ... 其他 TimeRange 封装 ...

char* olive_timecode_from_rational(OliveRational time,
                                    OliveRational timebase,
                                    int display_mode) {
    try {
        olive::Rational t(time.num, time.den);
        olive::Rational tb(timebase.num, timebase.den);
        QString str = olive::Timecode::time_to_string(
            t, tb,
            static_cast<olive::Timecode::Display>(display_mode)
        );
        QByteArray utf8 = str.toUtf8();
        char* result = static_cast<char*>(malloc(utf8.size() + 1));
        memcpy(result, utf8.constData(), utf8.size() + 1);
        return result;
    } catch (...) {
        SetError(OLIVE_ERROR_GENERIC, "Timecode conversion failed");
        return nullptr;
    }
}

OliveSampleBuffer* olive_sample_buffer_create(OliveAudioParams params, int sample_count) {
    try {
        // olive::AudioParams cpp_params = ...;
        auto* buf = new OliveSampleBuffer();
        // buf->impl = new olive::SampleBuffer(cpp_params, sample_count);
        return buf;
    } catch (...) {
        SetError(OLIVE_ERROR_NOMEM, "Failed to create SampleBuffer");
        return nullptr;
    }
}

void olive_sample_buffer_destroy(OliveSampleBuffer* buf) {
    if (buf) {
        delete buf;
    }
}

// ... 其他 SampleBuffer 封装 ...

}  // extern "C"
```

---

## 3. CMake 改造

### 3.1 `ext/core/CMakeLists.txt`

```cmake
# 改造前
# add_library(olivecore STATIC ...)

# 改造后
set(CORE_SOURCES
  src/rational.cpp
  src/color.cpp
  src/timerange.cpp
  src/samplebuffer.cpp
  src/pixelformat.cpp
  src/timecodefunctions.cpp
  # ... 其他源文件
)

# C API 封装层
set(CORE_API_SOURCES
  ${CMAKE_SOURCE_DIR}/c_api/src/core_api.cpp
)

add_library(olivecore SHARED
  ${CORE_SOURCES}
  ${CORE_API_SOURCES}
)

target_compile_definitions(olivecore PRIVATE OLIVE_BUILDING_CORE)

target_include_directories(olivecore
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/c_api/include
)

target_link_libraries(olivecore
  PUBLIC
    FFmpeg::avutil
    OpenGL::GL
    Imath::Imath
)

# 默认隐藏符号，只有标记 OLIVE_CORE_API 的才导出
set_target_properties(olivecore PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

# 安装
install(TARGETS olivecore DESTINATION lib)
install(DIRECTORY include/olive DESTINATION include)
install(FILES ${CMAKE_SOURCE_DIR}/c_api/include/olive/core_api.h DESTINATION include/olive)
```

---

## 4. 小步快跑实施步骤

### Step 0: 准备基础设施（1 天）

- [ ] 创建 `c_api/` 目录结构（`include/olive/`, `src/`, `tests/`）。
- [ ] 编写 `ModuleLoader` 类（`c_api/src/module_loader.h/cpp`），支持 POSIX + Windows。
- [ ] 在 CMake 中新增 `OLIVE_DYNAMIC_MODULES` 选项（默认 OFF）。
- [ ] 编写最小测试动态库，验证 `ModuleLoader` 可以正确加载和调用。

**验收标准**：`ModuleLoader` 可以成功 `dlopen` 一个测试 SO 并调用其中的函数。

### Step 1: 将 olivecore 改为 SHARED（1 天）

- [ ] 修改 `ext/core/CMakeLists.txt`：`STATIC` → `SHARED`，添加 `CXX_VISIBILITY_PRESET hidden`。
- [ ] 为需要导出的类/函数添加导出宏。
- [ ] 验证所有平台编译通过。

**验收标准**：`olivecore` 编译为 `.so`/`.dylib`/`.dll`，单元测试通过显式加载运行。

### Step 2: 编写 core C API（2–3 天）

- [ ] 编写 `c_api/include/olive/core_api.h`（先只包含最常用的类型：Rational, Color, TimeRange, PixelFormat）。
- [ ] 编写 `c_api/src/core_api.cpp`，用 C++ 封装现有类的调用，导出纯 C 函数。
- [ ] **原则**：不改变 `ext/core/` 下的任何现有源文件，只在 `c_api/src/` 中新增封装代码。
- [ ] 编写单元测试 `tests/c_api/test_core_api.cpp`。

**验收标准**：
```cpp
ModuleLoader loader;
loader.Load("core", "./libolivecore.so");
auto make = loader.GetFunction<OliveRational(*)(int64_t,int64_t)>("core", "olive_rational_make");
ASSERT_EQ(make(1, 2).num, 1);
```

### Step 3: 主进程加载验证（1 天）

- [ ] 在 `Core::Start()` 中新增代码：尝试显式加载 `libolivecore.so`，若失败则回退到静态链接模式。
- [ ] 验证主程序启动时 `olivecore` 被正确加载。

**验收标准**：主程序日志输出 `Loaded module: core from /path/to/libolivecore.so`。

### Step 4: 扩展 C API 覆盖度（按需）

- [ ] 根据其他模块（codec, node）的需要，逐步在 `core_api.h` 中增加类型（SampleBuffer, VideoParams 等）。

---

## 5. 风险与回退

| 风险 | 对策 |
|---|---|
| `ext/core` 的某些类依赖 Qt 模板，导出 C 接口繁琐 | 优先封装 POD 和简单类，复杂类（如 `SampleBuffer` 的音频重采样）暂时不对外暴露，留在内部使用。 |
| Windows 上 `__declspec(dllexport)` 与 `__attribute__((visibility))` 混用 | 定义统一的 `OLIVE_API` 宏，根据平台自动选择。 |
| 性能担忧：C 封装层增加函数调用开销 | `core` 中的操作（有理数运算）本身极快，C 封装的开销（一次函数调用）可忽略。若发现瓶颈，可将热点路径内联到 C API 头文件中（但保持 ABI 稳定）。 |

---

## 6. 与后续模块的协作

`libolivecore.so` 是最底层库，所有其他动态库（`olivecodec`, `olivenode`, `oliverender` 等）都隐式或显式依赖它。

- **隐式依赖**：`libolivecodec.so` 在编译时链接 `libolivecore.so`，运行时由操作系统加载器自动解析。
- **显式依赖**：主进程需要显式加载 `libolivecore.so`，然后才能加载依赖它的上层库（虽然操作系统加载器会自动处理 `DT_NEEDED`，但主进程仍需要显式 `dlopen` 以确保错误处理可控）。

**加载顺序**：
```cpp
loader.Load("core", path);    // 必须先加载
loader.Load("codec", path);   // 依赖 core，但操作系统会自动解析
loader.Load("node", path);    // 依赖 codec + core
```
