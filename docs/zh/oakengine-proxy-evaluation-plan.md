# OakEngine 跨模块 C++ 依赖清理：评估与实施计划

> 目标：将 `oakengine.so` 对 `oakcodec.so`、`oakgl.so`、`oakaudio.so` 的残余 C++ 直接依赖，全部改为"C 适配层 + Handle + Proxy 类"模式。  
> 前提：`docs/zh/modularization-plan.md` 中的部分阶段描述已不适用（`oakengine` 未拆出 `oaknodes`，`oakcodec` 已独立但 engine 仍有残留依赖）。本计划基于当前代码库实际状态制定。

---

## 一、现状评估

### 1.1 各模块 C API 成熟度

| 模块 | C API 头文件 | 实现完成度 | Runtime Loader |
|------|-------------|-----------|----------------|
| `oakcodec.so` | `include/oak/codec_api.h` + `frame_api.h` | ✅ 完整（`src/oakcodec/codec_api.cpp` 1359 行，覆盖 decoder/encoder/conform/frame） | ❌ 无（需新建） |
| `oakgl.so` | `include/oak/renderer_api.h` | ✅ 完整（`src/oakgl/renderer_api.cpp` 667 行） | ✅ `OakRendererRuntime` 已完成 |
| `oakaudio.so` | `include/oak/audio_api.h` | ✅ 完整（`src/oakaudio/audio_api.cpp`，19 个函数全实现） | ❌ 无（需新建） |
| `oakcolor.so` | `include/oak/color_api.h` | ✅ 完整 | 不需要（engine 未直接调用） |

### 1.2 Engine 中残余的 C++ 直接依赖

通过全文扫描，真正**实际使用**了外部模块 C++ 类型的文件如下：

#### 对 `oakcodec.so` 的依赖
| 文件 | 使用的类型/函数 | 严重程度 |
|------|----------------|---------|
| `task/conform/conform.h` | `Decoder::CodecStream` | 🔴 include 已 broken |
| `task/conform/conform.cpp` | `Decoder::CreateFromID`, `Decoder::IndexProgress` (信号) | 🔴 无法编译 |
| `render/rendercache.h` | `DecoderPtr`, `Decoder::CodecStream` | 🔴 include 已 broken |
| `render/renderprocessor.h/cpp` | `DecoderPtr`, `Decoder::CodecStream`, `Decoder::RetrieveVideoParams`, `Decoder::TransformImageSequenceFileName` | 🔴 无法编译 |
| `node/project/footage/footage.h/cpp` | `Decoder::Probe`, `Decoder::ReceiveListOfAllDecoders`, `FootageDescription` | 🔴 include 已 broken |
| `audio/audiomanager.h/cpp` | `FFmpegEncoder` | 🔴 include 已 broken |

> 另有 `node/block/clip/clip.h`、`node/traverser.h`、`node/output/viewer/viewer.h` 包含 `#include "decoder.h"`，但 grep 确认**无实际使用** `Decoder` 类型，属于 dangling include，可直接删除。

#### 对 `oakgl.so` 的依赖
| 文件 | 使用的类型/函数 | 严重程度 |
|------|----------------|---------|
| `render/rendermanager.cpp` | `OpenGLRenderer` 实例化（已被 stub 为 `nullptr`） | 🟡 渲染管道被禁用 |
| `render/renderprocessor.cpp` | `dynamic_cast<OpenGLRenderer*>` | 🟡 功能缺失 |
| `render/plugin/pluginrenderer.h` | 继承 `OpenGLRenderer` | 🟡 未来随 PluginRenderer 移出 engine |

#### 对 FFmpeg 的直接依赖（engine 内部代码）
| 文件 | 使用的 FFmpeg 头 | 严重程度 |
|------|-----------------|---------|
| `audio/audioprocessor.h` | `<libavfilter/avfilter.h>` | 🔴 违反模块化原则 |
| `audio/audiomanager.h` | 通过 `ffmpeg/ffmpegencoder.h` 间接引入 | 🔴 违反模块化原则 |

> `oakaudio.so` 的 C API 已提供 `OakAudioFilterGraphHandle`，完全可替代 `AudioProcessor` 中的 FFmpeg filter graph 逻辑。

### 1.3 结论：当前状态

`oakengine.so` 目前处于**半拆未拆、编译已坏**的状态：
- CMake 明确禁止 link 其他 `.so`
- 但源文件中仍大量 `#include` 其他模块的 C++ 头
- `src/engine/src/codec/` 目录为空，导致 `conform.h` 等编译失败
- `rendermanager.cpp` 中 `context_ = nullptr`，整个渲染管道停摆

**如果不采取 Proxy 方案，唯一替代方案是：**
1. 把 `oakcodec`/`oakaudio`/`oakgl` 的 C++ 头移入 `shared/include/`，恢复编译期链接；或
2. 把所有调用点重写为直接调用 C API（`oak_decoder_read_video(handle, ...)`），放弃 C++ 封装。

方案 1 违背模块化目标；方案 2 改动量极大（所有 `decoder->RetrieveVideo(...)` 都要改），且破坏现有代码结构。Proxy 方案是折中最优解。

---

## 二、方案评估：C 适配层 + Handle + Proxy 类

### 2.1 架构模式

```
┌─────────────────┐      dlopen/dlsym      ┌──────────────────┐
│   oakengine.so  │  ◄──────────────────►  │   oakcodec.so    │
│                 │   C API (extern "C")   │                  │
│  ┌───────────┐  │                      │  ┌──────────────┐  │
│  │DecoderProxy│ │  oak_decoder_read_video│  │ C Wrapper    │  │
│  │ (C++ class)│ │◄─────────────────────►│  │ (new/delete  │  │
│  │ 持 OakDecoder│ │                      │  │  + 成员函数转发)│  │
│  │   Handle)  │ │                      │  │              │  │
│  └───────────┘  │                      │  │  ┌────────┐  │  │
│                 │                      │  │  │Decoder │  │  │
│  业务逻辑几乎    │                      │  │  │(QObject)│  │  │
│  无需修改       │                      │  │  └────────┘  │  │
└─────────────────┘                      └──────────────────┘
```

- **被调模块侧（已存在）**：`oakcodec.so` 的 `codec_api.cpp` 已经是 C 适配层，用 `OakDecoderHandle`（`void*`）区分对象，内部转发给 C++ `Decoder` 成员函数。
- **主调模块侧（需新建）**：在 `oakengine.so` 中创建 `DecoderProxy` C++ 类，接口与原始 `Decoder` 保持一致，内部通过 `CodecRuntime`（dlopen loader）调用 C API。

### 2.2 性能损失评估

#### 2.2.1 调用开销模型（x86_64, 现代 CPU, 分支预测命中）

| 调用类型 | 典型耗时 | 说明 |
|---------|---------|------|
| 直接 C++ 非虚函数（可内联） | ~0-1 ns | 编译器内联后无 call 指令 |
| 直接 C++ 非虚函数（未内联） | ~1-3 ns | 直接 call，寄存器传参 |
| C++ 虚函数调用 | ~3-5 ns | 读取 vptr + 间接跳转 |
| 跨 SO C API（函数指针） | ~3-5 ns | 与虚函数调用同数量级 |
| **Proxy 模式总开销** | **~10-15 ns** | 虚函数/直接调用 → C 函数指针 → wrapper 转发 → C++ 成员函数 |

#### 2.2.2 各场景实际影响

| 场景 | 当前调用方式 | Proxy 后增加开销 | 单次操作基准耗时 | 相对损失 |
|------|------------|-----------------|----------------|---------|
| `Renderer::DrawQuad` | 虚函数 | ~+10 ns | GPU draw: 0.1-10 ms | **< 0.001%** |
| `Decoder::RetrieveVideo` | 虚函数 | ~+10 ns | 解码一帧: 5-50 ms | **< 0.001%** |
| `Decoder::RetrieveAudio` | 虚函数 | ~+10 ns | 解码一帧: 1-10 ms | **< 0.001%** |
| `AudioProcessor::Convert` | 直接调用 | ~+10 ns | FilterGraph 处理: 0.1-5 ms | **< 0.01%** |
| `NodeValue::GetInt` (内部) | 直接调用/内联 | — | — | **0%**（不涉及跨模块） |
| `RenderCache` 查找 | 模板内联 | — | Hash 查找: 50-200 ns | **0%**（不涉及跨模块） |

#### 2.2.3 关键结论

- **性能损失可忽略**：Proxy 模式引入的额外间接调用（~10 ns/次）相对于视频解码、GPU 渲染、磁盘 IO 等毫秒级操作，占比低于万分之一。
- **瓶颈未变**：视频编辑软件的性能瓶颈始终在于（1）视频解码器效率（2）GPU 带宽与着色器吞吐量（3）缓存 IO。函数调用开销不在关键路径上。
- **音频实时路径安全**：即使 48kHz/1024 sample 的实时音频（每帧 21ms），`AudioProcessor` 的 C API 调用是**批量处理**（一次 `process` 处理 1024 个 sample），而非逐 sample 调用，故调用开销可忽略。

### 2.3 可行性分析

#### 2.3.1 QObject / 信号槽跨模块

- **问题**：`Decoder` 继承 `QObject`，`ConformTask` 通过 Qt 信号槽连接 `Decoder::IndexProgress`。
- **分析**：Qt 的 meta-object 系统跨动态库在技术上可行（若两边链接同一 Qt），但违背"仅通过 C API"的原则，且 Rust 替换后不可用。
- **对策**：在 `oak/codec_api.h` 中增加回调接口：
  ```c
  typedef void (*OakDecoderProgressCallback)(double progress, void* userdata);
  void oak_decoder_set_progress_callback(OakDecoderHandle dec,
                                         OakDecoderProgressCallback cb,
                                         void* userdata);
  ```
  `DecoderProxy` 可在内部将 C 回调桥接为 Qt 信号（如果需要保持 `ConformTask` 的信号连接方式），也可直接让 `ConformTask` 持有一个 lambda 回调。

#### 2.3.2 智能指针与生命周期

- **问题**：`DecoderPtr` = `std::shared_ptr<Decoder>`，engine 中多处使用 shared_ptr 语义。
- **分析**：C API 当前使用裸 `OakDecoderHandle`（`void*`），无内置引用计数。
- **对策**：
  - 在 Proxy 类中采用**独占所有权**（`std::unique_ptr<DecoderProxy, DecoderProxyDeleter>`），因为 engine 中 `Decoder` 的使用模式实际上是 unique（`RenderCache` 持有，超时清理）。
  - 若确实需要共享，可给 C API 补充 `oak_decoder_retain(handle)` / `oak_decoder_release(handle)`，Proxy 内部用 `std::shared_ptr` 包装 handle。
  - **推荐**：先采用 unique 语义，最小化改动；若后续发现需要共享，再补充 retain/release。

#### 2.3.3 POD 数据类型

- **问题**：`Decoder::CodecStream`（QString + int）被用作 `RenderCache` 的模板 Key；`FootageDescription` 被 `Footage` 类使用。
- **分析**：二者均为纯数据类型，无行为依赖。
- **对策**：
  - `CodecStream`：在 engine 中复制一个等价定义（或直接用 `std::pair<QString, int>`），无需 C API。
  - `FootageDescription`：可在 engine 侧保留相同定义（它是 POD），或者通过 C API 的 `OakMediaInfo` 转换。由于它不依赖 `Decoder` 的运行时行为，保留同构定义不会破坏模块化。

#### 2.3.4 模板与内联

- **问题**：`RenderCache<K, V>` 是模板，要求 Key/Value 类型编译期可见。
- **分析**：`RenderCache` 在 engine 内部，不涉及跨模块。只要 `CodecStream` 定义在 engine 中可见即可。
- **对策**：如上，复制 `CodecStream` 定义到 engine。

### 2.4 是否值得？

**结论：非常值得。**

| 维度 | 评分 | 理由 |
|------|------|------|
| 代码改动幅度 | ⭐⭐⭐⭐⭐ | 业务逻辑几乎不用改，仅替换 include 和类型名；C API 已完备 |
| 性能影响 | ⭐⭐⭐⭐⭐ | < 0.01%，可忽略 |
| ABI 隔离 | ⭐⭐⭐⭐⭐ | 模块可独立替换、升级、Rust 化 |
| 渐进式可行性 | ⭐⭐⭐⭐⭐ | 可按类逐个替换，无需大爆炸重构 |
| 维护成本 | ⭐⭐⭐⭐ | 需维护 Proxy 类，但接口稳定后变动极少 |
| 信号槽复杂度 | ⭐⭐⭐ | 需将 Qt 信号改为 C 回调，一处（`IndexProgress`） |

唯一可行的替代方案（全部重写为裸 C API 调用）会导致：
- 所有 `decoder->RetrieveVideo(p)` 改为 `oak_decoder_read_video(handle, &p, ...)`
- 所有 `renderer->DrawQuad(...)` 改为 `oak_renderer_draw_quad(handle, ...)`
- 改动量估计为 Proxy 方案的 **5-10 倍**，且严重破坏代码可读性。

---

## 三、实施计划

### 3.1 边界与范围

- **本次修改范围**：`src/engine/` 目录内的文件，以及必要的 C API 补充（`include/oak/*.h`）。
- **不修改范围**：
  - `src/oakcodec/`, `src/oakgl/`, `src/oakaudio/` 的内部 C++ 实现（除非需要补充 C API 函数）。
  - `oaknodes`/`oakcore`/`oakpluginhost`：当前未独立为 `.so`，无需处理。
  - `PluginRenderer`：按用户指示，先不动，最终移入 `oakrenderer` 可执行文件。
  - `ColorProcessor` / `ColorManager`：按用户指示，color 保留在 engine 中，OCIO ABI 稳定，不强制拆出。

### 3.2 阶段划分

#### Phase 1：OpenGLRendererProxy — 恢复渲染管道（2-3 天）

**目标**：让 `rendermanager.cpp` 重新实例化渲染器，渲染线程正常工作。

| 动作 | 新增/修改文件 | 说明 |
|------|-------------|------|
| 1. 实现 `OpenGLRendererProxy.cpp` | **新建** `src/engine/src/render/opengl/opengl_renderer_proxy.cpp` | 基于已有头文件，通过 `OakRendererRuntime` 调用 C API；`QVariant` native handle 用 `void*` 包装 |
| 2. 恢复渲染器实例化 | 修改 `src/engine/src/render/rendermanager.cpp` | `context_ = new OpenGLRendererProxy()` 替代 `nullptr` |
| 3. 处理 `renderprocessor.cpp` | 修改 `src/engine/src/render/renderprocessor.cpp` | 移除 `dynamic_cast<OpenGLRenderer*>`，改用 `Renderer*` 基类接口或判断 backend name |
| 4. 移除旧实现 | 修改 `src/engine/src/render/opengl/CMakeLists.txt` | 移除 `openglrenderer.cpp/h` 编译；保留 `opengl_renderer_proxy.h/cpp` |
| 5. 验证 | 构建 + 运行 | 确认渲染线程启动，UI 有画面输出 |

#### Phase 2：DecoderProxy + CodecRuntime — 切断 oakcodec C++ 依赖（4-6 天）

**目标**：engine 中所有 `Decoder`/`DecoderPtr`/`FFmpegEncoder`/`FootageDescription` 直接依赖全部消除。

| 动作 | 新增/修改文件 | 说明 |
|------|-------------|------|
| 1. 新建 CodecRuntime Loader | **新建** `src/engine/src/runtime/oak_codec_runtime.h/cpp` | 仿照 `OakRendererRuntime`，`dlopen("liboakcodec.dylib/so")`，加载 `oak/codec_api.h` 全部符号 |
| 2. 新建 DecoderProxy | **新建** `src/engine/src/codec/decoder_proxy.h/cpp` | 接口模拟 `Decoder`：静态方法 `CreateFromID`、`ReceiveListOfAllDecoders`、`Probe`、`TransformImageSequenceFileName`；实例方法 `Open`、`RetrieveVideo`、`RetrieveAudio`、`ConformAudio`、`Close`；内部持 `OakDecoderHandle`；增加 progress callback 注册接口 |
| 3. 新建 EncoderProxy | **新建** `src/engine/src/codec/encoder_proxy.h/cpp` | 包装 `oak_encoder_*` C API，供 `AudioManager` 录制功能使用 |
| 4. 复制/适配 POD 类型 | **新建** `src/engine/src/codec/codec_stream.h` | 复制 `Decoder::CodecStream` 定义（QString + int + hash） |
| 5. 复制 FootageDescription | **新建** `src/engine/src/codec/footage_description.h` | 复制原 `footagedescription.h` 内容（纯 POD，无外部依赖） |
| 6. 补充 C API（如需） | 修改 `include/oak/codec_api.h` + `src/oakcodec/codec_api.cpp` | 增加 `oak_decoder_set_progress_callback`、`oak_decoder_retain/release`（若需要 shared_ptr 语义） |
| 7. 替换 engine 中的 include | 修改以下文件 | |
| | `render/rendercache.h` | `#include "decoder.h"` → `#include "codec/decoder_proxy.h"`；`DecoderPair` 改持 `DecoderProxyPtr` |
| | `render/renderprocessor.h/cpp` | `DecoderPtr` → `DecoderProxyPtr`；`ResolveDecoderFromInput` 返回类型更新 |
| | `task/conform/conform.h/cpp` | 使用 `DecoderProxy`；`IndexProgress` 改为 callback/lambda |
| | `node/project/footage/footage.h/cpp` | 使用 `DecoderProxy` 静态方法；`FootageDescription` 使用 engine 内定义 |
| | `audio/audiomanager.h/cpp` | `FFmpegEncoder` → `EncoderProxy` |
| 8. 删除 dangling includes | 修改 `clip.h`、`traverser.h`、`viewer.h` | 直接移除 `#include "decoder.h"` |
| 9. 验证 | 构建 + 运行 | 确认媒体导入、解码、conform、录制功能正常 |

#### Phase 3：AudioProcessorProxy — 切断 engine 中 FFmpeg 依赖（2-3 天）

**目标**：engine 不再直接包含任何 FFmpeg 头文件；音频处理通过 `oakaudio.so` C API。

| 动作 | 新增/修改文件 | 说明 |
|------|-------------|------|
| 1. 新建 AudioRuntime Loader | **新建** `src/engine/src/runtime/oak_audio_runtime.h/cpp` | `dlopen("liboakaudio.dylib/so")`，加载 `oak/audio_api.h` 全部符号 |
| 2. 新建 AudioProcessorProxy | **新建** `src/engine/src/audio/audio_processor_proxy.h/cpp` | 接口与现有 `AudioProcessor` 保持一致：`Open(from, to, tempo)`、`Convert`、`Flush`、`Close`；内部持 `OakAudioFilterGraphHandle`，调用 C API |
| 3. 替换 engine 中使用点 | 修改 `src/engine/src/audio/audioprocessor.h/cpp` | 保留头文件/源文件名，内部实现改为 Proxy（或新建 proxy 文件，删除旧实现） |
| | 修改 `src/engine/src/node/output/track/track.cpp` | `AudioProcessor` → `AudioProcessorProxy`（通常只需改类型名） |
| | 修改 `src/engine/src/render/renderprocessor.cpp` | 同上 |
| 4. 清理 FFmpeg include | 修改 `src/engine/CMakeLists.txt` | 从 engine 的 include/link 中移除 `libavfilter` 等 FFmpeg 组件（确认已由 oakaudio.so 私有链接） |
| 5. 验证 | 构建 + 运行 | 确认音频回放、变速、pitch shift 正常 |

#### Phase 4：清理、验证与 CI 防护（1-2 天）

| 动作 | 文件 | 说明 |
|------|------|------|
| 1. 编译边界扫描脚本 | 修改 `.github/workflows/ci.yml` | 增加步骤：扫描 `src/engine/` 中是否包含 `#include <libav*>`、`#include <libsw*>`、`#include <OpenEXR/`、`#include <QOpenGL*`（视策略而定） |
| 2. 链接边界检查 | 修改 `src/engine/CMakeLists.txt` | 确保 `target_link_libraries(oakengine)` 中没有 `oakcodec`、`oakaudio`、`oakgl`、`oakcolor` |
| 3. 运行现有测试 | `tests/` | 确保 compositing/timeline/general 测试通过 |
| 4. 文档更新 | 修改 `docs/zh/engine-so-migration.md` | 记录本次完成的 Proxy 工作 |

### 3.3 新增文件清单

```
src/engine/src/
├── runtime/
│   ├── oak_codec_runtime.h          (仿 OakRendererRuntime)
│   ├── oak_codec_runtime.cpp
│   ├── oak_audio_runtime.h
│   └── oak_audio_runtime.cpp
├── codec/
│   ├── decoder_proxy.h              (模拟 Decoder 接口)
│   ├── decoder_proxy.cpp
│   ├── encoder_proxy.h              (模拟 FFmpegEncoder 接口)
│   ├── encoder_proxy.cpp
│   ├── codec_stream.h               (POD: QString+int)
│   └── footage_description.h        (POD: 复制原定义)
├── audio/
│   ├── audio_processor_proxy.h      (模拟 AudioProcessor 接口)
│   └── audio_processor_proxy.cpp
└── render/opengl/
    └── opengl_renderer_proxy.cpp    (补完已有头文件)
```

### 3.4 修改文件清单（预估）

```
src/engine/src/
├── render/rendermanager.cpp
├── render/renderprocessor.cpp
├── render/renderprocessor.h
├── render/rendercache.h
├── render/opengl/CMakeLists.txt
├── task/conform/conform.cpp
├── task/conform/conform.h
├── node/project/footage/footage.cpp
├── node/project/footage/footage.h
├── node/block/clip/clip.h
├── node/traverser.h
├── node/output/viewer/viewer.h
├── audio/audiomanager.cpp
├── audio/audiomanager.h
├── audio/audioprocessor.cpp
├── audio/audioprocessor.h
├── node/output/track/track.cpp
└── CMakeLists.txt

include/oak/
└── codec_api.h                  (补充 callback / retain-release 接口)
```

### 3.5 时间估算

| 阶段 | 预估工期 | 阻塞关系 |
|------|---------|---------|
| Phase 1：Renderer Proxy | 2-3 天 | 无 |
| Phase 2：Decoder/Encoder Proxy | 4-6 天 | 无（可与 P1 部分并行设计） |
| Phase 3：AudioProcessor Proxy | 2-3 天 | 无 |
| Phase 4：清理与验证 | 1-2 天 | 依赖 P1-P3 |
| **总计** | **约 2 周** | |

---

## 四、风险与对策

| 风险 | 可能性 | 影响 | 对策 |
|------|--------|------|------|
| `DecoderProxy` 接口与原始 `Decoder` 不完全一致，导致某处调用编译失败 | 中 | 中 | 采用"编译驱动"策略：每替换一个 include，立即编译，修复报错 |
| `FootageDescription` 复制后与 `oakcodec.so` 内部版本出现语义漂移 | 低 | 中 | `FootageDescription` 是只读 POD，engine 只读不写；后续可用 C API 的 `OakMediaInfo` 完全替代 |
| `AudioProcessorProxy` 的 `Convert` 返回类型（`QVector<QByteArray>`）与 C API 不兼容 | 中 | 中 | Proxy 内部做数据格式转换（`float*` ↔ `QByteArray`），逻辑与旧实现一致 |
| `OpenGLRendererProxy` 的 `QVariant` native handle 与 C API `void*` 互操作问题 | 低 | 高 | 已确认 `Renderer` 虚函数使用 `QVariant` 传递 handle；Proxy 层用 `QVariant::fromValue<void*>()` / `value<void*>()` 包装即可 |
| 构建系统缓存导致旧 object 文件残留 | 低 | 低 | 清理 build 目录后全量重编 |

---

## 五、下一步

本评估与计划提交审批。获批后，按 Phase 1 → Phase 2 → Phase 3 → Phase 4 顺序执行，每阶段完成后提交阶段性验证结果。
