# liboliverender.so — 渲染引擎抽象

> **依赖**：`libolivecore.so`, `libolivecodec.so`, `libolivenode.so`  
> **外部依赖**：Qt::Core, Qt::OpenGL, OpenColorIO, OpenGL  
> **包含源码**：`app/render/`（不含 OpenGL 具体后端平台代码的抽象层）  
> **当前状态**：单体 OBJECT 库的一部分，直接管理 `RenderThread`、`RenderProcessor`、`OpenGLRenderer`  
> **改造难度**：⭐⭐⭐⭐（困难，与 Node 耦合深）

---

## 1. 当前状态分析

`app/render/` 是渲染系统的核心，负责将节点图转换为可显示的帧/音频。当前架构：

| 组件 | 说明 |
|---|---|
| `rendermanager.h/cpp` | 渲染管理单例，管理 `RenderThread` 和缓存 |
| `renderprocessor.h/cpp` | 节点图遍历 + 渲染作业生成（继承 `NodeTraverser`） |
| `renderer.h/cpp` | 渲染器抽象基类（`Renderer`） |
| `opengl/openglrenderer.h/cpp` | OpenGL 渲染后端 |
| `job/*.h` | 各种渲染作业类型（ShaderJob, FootageJob, GenerateJob 等） |
| `previewautocacher.h/cpp` | 预览自动缓存 |
| `rendercache.h/cpp` | 渲染缓存框架 |

**关键设计决策**：

本方案中，**实际的 GPU 渲染发生在 `olive-renderer` 子进程中**，不在主进程的 `liboliverender.so` 中。因此 `liboliverender.so` 的角色需要重新定位：

- **在主进程中**：`liboliverender.so` 提供轻量的 **渲染客户端** 功能：节点图序列化、渲染参数打包、共享内存创建、子进程启动协调。
- **在子进程中**：`olive-renderer` 可执行文件链接 `liboliverender.so`（或静态链接其代码），执行实际的 `RenderProcessor` + `OpenGLRenderer`。

也就是说，`liboliverender.so` 既服务于主进程（IPC 客户端），也服务于子进程（渲染服务端）。但通过编译选项或子目录拆分，可以在主进程中只包含轻量客户端代码。

**简化方案**：`liboliverender.so` 包含完整的渲染逻辑（包括 `RenderProcessor` 和 `Renderer` 抽象），但主进程中的 `RenderManager` 不再直接调用它，而是通过 C API 启动 `olive-renderer` 子进程。子进程自身可以静态链接或动态链接 `liboliverender.so` 来执行渲染。

---

## 2. C API 设计

### 2.1 头文件：`c_api/include/olive/render_api.h`

```c
#ifndef OLIVE_RENDER_API_H
#define OLIVE_RENDER_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core_api.h"
#include "node_api.h"

#define OLIVE_RENDER_API_VERSION 1

#ifdef OLIVE_BUILDING_RENDER
#  define OLIVE_RENDER_API __attribute__((visibility("default")))
#else
#  define OLIVE_RENDER_API
#endif

/* ========== 枚举 ========== */
typedef enum {
    OLIVE_RENDER_MODE_OFFLINE = 0,  // 最高质量（导出）
    OLIVE_RENDER_MODE_ONLINE,        // 实时预览（允许降低精度）
} OliveRenderMode;

typedef enum {
    OLIVE_RENDER_BACKEND_OPENGL = 0,
    OLIVE_RENDER_BACKEND_DUMMY,
} OliveRenderBackend;

/* ========== 不透明类型 ========== */
typedef struct OliveRenderContext   OliveRenderContext;
typedef struct OliveRenderTicket    OliveRenderTicket;
typedef struct OliveRenderParams    OliveRenderParams;

/* ========== 渲染参数结构体 ========== */
typedef struct {
    OliveNodeGraph* node_graph;
    const char* output_node_id;      // 通常为 ViewerOutput 的 ID
    OliveRational time;
    OliveVideoParams video_params;
    OliveAudioParams audio_params;
    OliveRenderMode mode;
    OliveRenderBackend backend;
    const char* color_reference_space;   // 可为 nullptr
    const char* color_display_space;     // 可为 nullptr
    OliveSize force_size;                // {0,0} 表示不强制
    OlivePixelFormat force_format;       // INVALID 表示不强制
} OliveRenderFrameParams;

typedef struct {
    OliveNodeGraph* node_graph;
    const char* output_node_id;
    OliveRational start;
    OliveRational duration;
    OliveAudioParams audio_params;
    OliveRenderMode mode;
} OliveRenderAudioParams;

/* ========== API 版本 ========== */
OLIVE_RENDER_API int olive_render_api_version(void);

/* ========== 渲染上下文（用于本地/同进程渲染，或子进程内部） ========== */
OLIVE_RENDER_API OliveRenderContext* olive_render_context_create(OliveRenderBackend backend);
OLIVE_RENDER_API void olive_render_context_destroy(OliveRenderContext* ctx);
OLIVE_RENDER_API int olive_render_context_init(OliveRenderContext* ctx);

/* ========== 同步渲染（单帧） ========== */
// 渲染视频帧，结果写入 out_frame（OliveFrame*，定义在 codec_api.h）
OLIVE_RENDER_API int olive_render_frame_sync(OliveRenderContext* ctx,
                                              const OliveRenderFrameParams* params,
                                              void** out_frame_data,   // 原始像素数据，需 olive_core_free
                                              size_t* out_frame_size,
                                              int* out_width,
                                              int* out_height,
                                              OlivePixelFormat* out_format);

// 渲染音频，结果写入 out_buffer（OliveSampleBuffer*，定义在 core_api.h）
OLIVE_RENDER_API int olive_render_audio_sync(OliveRenderContext* ctx,
                                              const OliveRenderAudioParams* params,
                                              OliveSampleBuffer** out_buffer);

/* ========== 异步渲染接口（用于子进程模型中的本地队列） ========== */
OLIVE_RENDER_API OliveRenderTicket* olive_render_frame_async(OliveRenderContext* ctx,
                                                               const OliveRenderFrameParams* params);
OLIVE_RENDER_API OliveRenderTicket* olive_render_audio_async(OliveRenderContext* ctx,
                                                               const OliveRenderAudioParams* params);
OLIVE_RENDER_API int olive_render_ticket_wait(OliveRenderTicket* ticket, int timeout_ms);
OLIVE_RENDER_API int olive_render_ticket_get_result_frame(OliveRenderTicket* ticket,
                                                             void** out_frame_data,
                                                             size_t* out_frame_size,
                                                             int* out_width,
                                                             int* out_height,
                                                             OlivePixelFormat* out_format);
OLIVE_RENDER_API void olive_render_ticket_destroy(OliveRenderTicket* ticket);
OLIVE_RENDER_API void olive_render_cancel_ticket(OliveRenderTicket* ticket);

/* ========== 节点图序列化辅助（供子进程使用） ========== */
// 将节点图序列化为适合渲染子进程消费的紧凑格式
OLIVE_RENDER_API char* olive_render_serialize_graph_for_render(OliveNodeGraph* graph,
                                                                 const char* output_node_id,
                                                                 size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif  // OLIVE_RENDER_API_H
```

### 2.2 实现要点

```cpp
// c_api/src/render_api.cpp

#include "olive/render_api.h"
#include "render/rendermanager.h"
#include "render/renderprocessor.h"
#include "render/renderer.h"
#include "render/opengl/openglrenderer.h"
#include "render/job/shaderjob.h"
#include "render/job/generatejob.h"
#include "render/job/footagejob.h"
#include "render/job/colortransformjob.h"
#include "render/job/samplejob.h"
#include "codec/frame.h"
#include "node/output/viewer/viewer.h"
#include "node/project.h"

struct OliveRenderContext {
    olive::Renderer* renderer = nullptr;
    olive::DecoderCache* decoder_cache = nullptr;
    olive::ShaderCache* shader_cache = nullptr;
};

struct OliveRenderTicket {
    olive::RenderTicketPtr impl;
};

extern "C" {

OliveRenderContext* olive_render_context_create(OliveRenderBackend backend) {
    try {
        auto* ctx = new OliveRenderContext();
        if (backend == OLIVE_RENDER_BACKEND_OPENGL) {
            ctx->renderer = new olive::OpenGLRenderer();
        } else {
            // ctx->renderer = new olive::DummyRenderer();
        }
        ctx->decoder_cache = new olive::DecoderCache();
        ctx->shader_cache = new olive::ShaderCache();
        return ctx;
    } catch (...) {
        return nullptr;
    }
}

void olive_render_context_destroy(OliveRenderContext* ctx) {
    if (!ctx) return;
    delete ctx->shader_cache;
    delete ctx->decoder_cache;
    if (ctx->renderer) {
        ctx->renderer->Destroy();
        delete ctx->renderer;
    }
    delete ctx;
}

int olive_render_context_init(OliveRenderContext* ctx) {
    if (!ctx || !ctx->renderer) return OLIVE_ERROR_INVALID;
    try {
        if (!ctx->renderer->Init()) return OLIVE_ERROR_GENERIC;
        ctx->renderer->PostInit();
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

int olive_render_frame_sync(OliveRenderContext* ctx,
                             const OliveRenderFrameParams* params,
                             void** out_frame_data,
                             size_t* out_frame_size,
                             int* out_width,
                             int* out_height,
                             OlivePixelFormat* out_format) {
    if (!ctx || !params) return OLIVE_ERROR_INVALID;
    try {
        // 1. 找到输出节点
        olive::Node* output_node = nullptr;
        {
            auto* cpp_graph = static_cast<olive::NodeGraph*>(params->node_graph); // 需要内部转换
            // ... 查找 output_node_id 对应的节点 ...
        }

        // 2. 构造 RenderVideoParams
        olive::RenderManager::RenderVideoParams vparams(
            output_node,
            ConvertToCpp(params->video_params),
            ConvertToCpp(params->audio_params),
            olive::Rational(params->time.num, params->time.den),
            nullptr,  // ColorManager，需从 graph 获取或传入
            params->mode == OLIVE_RENDER_MODE_OFFLINE ? olive::RenderMode::kOffline : olive::RenderMode::kOnline
        );

        // 3. 创建 ticket 并执行
        auto ticket = std::make_shared<olive::RenderTicket>();
        ticket->Start();
        olive::RenderProcessor::Process(ticket, ctx->renderer, ctx->decoder_cache, ctx->shader_cache);

        // 4. 等待结果
        ticket->WaitForFinished();
        if (!ticket->HasResult()) return OLIVE_ERROR_GENERIC;

        // 5. 提取帧数据
        olive::FramePtr frame = ticket->Get().value<olive::FramePtr>();
        if (!frame) return OLIVE_ERROR_GENERIC;

        *out_width = frame->width();
        *out_height = frame->height();
        *out_format = ConvertToC(frame->format());

        size_t data_size = frame->allocated_size();
        void* data = malloc(data_size);
        memcpy(data, frame->data(), data_size);
        *out_frame_data = data;
        *out_frame_size = data_size;

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
# app/render/CMakeLists.txt

set(RENDER_INTERNAL_SOURCES
  rendermanager.cpp rendermanager.h
  renderprocessor.cpp renderprocessor.h
  renderer.cpp renderer.h
  renderticket.cpp renderticket.h
  rendercache.cpp rendercache.h
  previewautocacher.cpp previewautocacher.h
  colorprocessor.cpp colorprocessor.h
  colorprocessorcache.cpp colorprocessorcache.h
  diskmanager.cpp diskmanager.h
  # ... job/ 目录下的所有文件
  job/shaderjob.cpp job/shaderjob.h
  job/generatejob.cpp job/generatejob.h
  job/footagejob.cpp job/footagejob.h
  job/colortransformjob.cpp job/colortransformjob.h
  job/samplejob.cpp job/samplejob.h
  job/cachejob.cpp job/cachejob.h
  job/pluginjob.cpp job/pluginjob.h
  job/acceleratedjob.cpp job/acceleratedjob.h
  # ... opengl/ 目录
  opengl/openglrenderer.cpp opengl/openglrenderer.h
  opengl/openglshader.cpp opengl/openglshader.h
  opengl/opengltexture.cpp opengl/opengltexture.h
  # ... plugin/ 目录（OFX 插件专用渲染器）
  plugin/pluginrenderer.cpp plugin/pluginrenderer.h
)

set(RENDER_API_SOURCES
  ${CMAKE_SOURCE_DIR}/c_api/src/render_api.cpp
)

add_library(oliverender SHARED
  ${RENDER_INTERNAL_SOURCES}
  ${RENDER_API_SOURCES}
)

target_compile_definitions(oliverender PRIVATE OLIVE_BUILDING_RENDER)

target_include_directories(oliverender
  PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/third_party/openfx/include
    ${CMAKE_SOURCE_DIR}/third_party/openfx/HostSupport/include
    ${CMAKE_SOURCE_DIR}/c_api/include
  PUBLIC
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(oliverender
  PUBLIC
    olivenode
    olivecodec
    olivecore
    oliveplugin
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::OpenGL
    ${OCIO_LIBRARIES}
    OpenGL::GL
)

set_target_properties(oliverender PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

install(TARGETS oliverender DESTINATION lib)
install(FILES ${CMAKE_SOURCE_DIR}/c_api/include/olive/render_api.h DESTINATION include/olive)
```

---

## 4. 小步快跑实施步骤

### Step 0: 分离渲染客户端与服务端代码（2 天）

- [ ] 分析 `render/` 中哪些代码主进程需要（RenderManager 的排队/协调逻辑），哪些仅子进程需要（OpenGLRenderer、RenderProcessor）。
- [ ] 创建 `app/render/client/` 子目录，放置主进程专用的轻量代码（如 `RenderProcessLauncher`）。
- [ ] 确保 `render/` 的现有代码仍然可以编译为完整的库（子进程使用）。

**验收标准**：`liboliverender.so` 编译成功，包含完整的渲染逻辑。

### Step 1: 将 render/ 独立为动态库（1 天）

- [ ] 修改 `app/render/CMakeLists.txt`，将 `render/` 从主 OBJECT 库移出，构建为 `oliverender SHARED`。
- [ ] 处理 `shaders/` 目录的资源文件路径问题（子进程需要知道着色器文件位置）。

**验收标准**：`liboliverender.so` 编译成功。

### Step 2: 最小 C API（2 天）

- [ ] 实现 `olive_render_context_create/destroy/init`。
- [ ] 实现 `olive_render_frame_sync`（同步渲染单帧）。
- [ ] 此 C API 主要供 `olive-renderer` 子进程内部使用（子进程加载 `liboliverender.so` 后调用）。

**验收标准**：可以编写一个命令行测试程序，加载 `liboliverender.so`，初始化 OpenGL，渲染一帧纯色。

### Step 3: 节点图序列化辅助（1 天）

- [ ] 实现 `olive_render_serialize_graph_for_render`。
- [ ] 此函数供主进程调用，将目标 `ViewerOutput` 及其上游节点序列化为紧凑 XML。

**验收标准**：给定一个包含 ViewerOutput 的图，序列化后的 XML 可以被 `ProjectSerializer` 重新加载。

### Step 4: 异步 Ticket 接口（2 天）

- [ ] 实现 `olive_render_frame_async`, `olive_render_ticket_wait`, `olive_render_ticket_get_result_frame`。
- [ ] 此接口用于子进程内部的并发渲染（一个子进程内可同时渲染多帧）。

---

## 5. 风险与回退

| 风险 | 对策 |
|---|---|
| `RenderProcessor` 深度依赖 `NodeTraverser`，C API 难以表达遍历逻辑 | `olive_render_frame_sync` 是高阶封装，内部直接使用原有的 C++ `RenderProcessor`，C API 调用者无需了解遍历细节。 |
| OpenGL 上下文初始化在不同平台差异大 | 在子进程中处理平台差异（子进程使用 `QOffscreenSurface` + `QOpenGLContext`）。C API 中 `backend` 参数暂时只支持 `"opengl"`。 |
| `PreviewAutoCacher` 的复杂缓存逻辑 | `PreviewAutoCacher` 保留在主进程中（或完全移除，因为"用完即弃"的渲染模型下，缓存策略由主进程重新设计）。 |
| 子进程需要访问 `app/shaders/` 下的 GLSL 文件 | 通过命令行参数 `--shader-path` 将资源路径传递给子进程。打包时确保着色器文件与可执行文件一同分发。 |
| `ColorManager` 和 OCIO 配置 | 通过 C API 参数 `color_reference_space` / `color_display_space` 传递，子进程内部重建 `ColorManager`。 |
