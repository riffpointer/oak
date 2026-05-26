# Oak 模块间依赖关系与架构设计

> 本文阐述 Oak 编辑器各 `.so` 模块之间的依赖规则、通信方式、设计动机与工程实践。

---

## 一、模块全景

Oak 从单体架构重构为以下独立动态库（`.so` / `.dylib`）模块：

| 模块 | 动态库 | 职责 | 核心依赖 |
|------|--------|------|----------|
| **oakshared** | `liboakshared.a`（静态） | 基础类型、工具函数、POD 结构 | Qt Core/Gui |
| **oakcodec** | `liboakcodec.dylib` | 媒体编解码（FFmpeg、OpenImageIO） | oakshared, FFmpeg, OIIO |
| **oakcolor** | `liboakcolor.dylib` | 色彩管理（OpenColorIO） | oakshared, OCIO |
| **oakgl** | `liboakgl.dylib` | 渲染后端（OpenGL） | oakshared, Qt OpenGL |
| **oakaudio** | `liboakaudio.dylib` | 音频处理（PortAudio） | oakshared, PortAudio |
| **oakengine** | `liboakengine.dylib` | 节点图、项目、渲染会话、OFX Host | oakshared, OfxHost, Qt |
| **app** | `Olive.app` | UI 主程序（Qt Widgets / QML） | 所有模块 |

**关键规则**：`oakcodec`、`oakcolor`、`oakgl`、`oakaudio`、`oakengine` 之间**禁止直接链接**。所有跨模块调用必须通过 `extern "C"` 的 C API，且优先使用 `dlopen` + `dlsym` 动态解析。

---

## 二、为什么用 `dlopen` + C API？

### 2.1 解耦与独立演化

在单体架构中，`engine/src/render/` 直接 `#include <libavcodec/avcodec.h>`，`app/` 直接链接 `OpenColorIO`，导致：
- 升级 FFmpeg 8.x → 9.x 时，全项目重新编译。
- 替换 OCIO 为自研色彩系统时，波及 50+ 文件。
- 引入 Metal / Vulkan 后端时，OpenGL 代码散落在 engine、app、render 三个目录。

通过 `dlopen` + C API：
- `oakcodec.so` 是 FFmpeg 的**唯一合法入口**。其他模块甚至不知道 FFmpeg 的存在。
- `oakgl.so` 是 OpenGL 的**唯一合法入口**。`oakengine` 通过 `OakRendererRuntime`（运行时加载器）调用渲染函数，内部完全不包含 OpenGL 头文件。
- 每个模块可以独立升级、替换、甚至完全重写，只要 C API 契约不变。

### 2.2 避免符号冲突

Qt、FFmpeg、OCIO、OIIO 都有大量全局符号和静态初始化器。直接链接到同一个可执行文件时：
- FFmpeg 的 `av_log_set_callback` 可能被 Qt 的日志系统覆盖。
- OCIO 的静态配置缓存可能与 OIIO 的色彩配置冲突。
- 不同版本的 `libexpat`（OCIO 依赖 vs OFX Host 依赖）可能导致符号解析错误。

通过动态库隔离：
- 每个 `.so` 有自己的符号表和依赖链。
- `oakcodec.so` 内部静态链接或私有引用 FFmpeg，`oakcolor.so` 内部引用 OCIO，彼此不干扰。
- 主程序（`app`）只暴露 Qt 和 C API 符号，干净可控。

### 2.3 支持可选模块与插件化

未来 Oak 可能支持：
- **可选渲染后端**：用户机器没有 GPU 时，`oakgl.so` 可以不加载，引擎回退到 CPU 渲染。
- **第三方编码器插件**：如 NVENC、Apple ProRes 硬件编码器，以独立 `.so` 形式存在，通过 C API 注册到 `oakcodec.so`。
- **云渲染**：`oakengine.so` 可以在无 GUI 的服务器上运行，通过 `dlopen` 加载远程的 `oakgl.so`（基于 EGL / OSMesa）。

### 2.4 测试友好

C API 测试可以直接 `dlopen` 目标 `.so`，验证符号导出和运行时行为：
```cpp
void* so = dlopen("liboakcodec.dylib", RTLD_NOW);
auto* fn = (int(*)())dlsym(so, "oak_decoder_supports_video");
EXPECT_NE(fn, nullptr);
```

这种黑盒测试不依赖内部头文件，能真实反映模块的发布状态。

---

## 三、依赖规则详解

### 3.1 严格分层

```
┌─────────────────────────────────────────┐
│  app (UI)                               │
│  只允许调用 C API，禁止 include 内部头文件  │
├─────────────────────────────────────────┤
│  oakengine.so                           │
│  节点图、OFX Host、渲染会话调度             │
│  → 运行时 dlopen: oakcodec, oakcolor, oakgl │
├─────────────────────────────────────────┤
│  oakcodec.so   oakcolor.so   oakgl.so   │
│  各自封装 FFmpeg, OCIO, OpenGL          │
│  → 彼此不链接，不 include 对方内部头文件    │
├─────────────────────────────────────────┤
│  oakshared (static)                     │
│  基础类型、POD、工具函数                   │
│  → 被所有模块静态链接                       │
└─────────────────────────────────────────┘
```

### 3.2 跨模块调用方式

| 场景 | 方式 | 示例 |
|------|------|------|
| 同模块内部 | C++ 自由调用 | `olive::Decoder::Open()` |
| 模块 A → 模块 B | `extern "C"` C API | `oak_decoder_read_video()` |
| 模块 A 需要 B 的实例 | 不透明句柄 + C API | `OakRendererHandle` |
| 模块 A 需要 B 的回调 | C 函数指针 | `OakDecoderProgressCallback` |
| 模块 A 需要 B 的枚举 | 复制到公共头文件 | `OakFramePixelFormat` 在 `frame_api.h` 中定义 |

### 3.3 禁止的行为

以下行为在 CI 中通过 `grep` 脚本检测，违规即构建失败：

1. **非 oakcodec 模块包含 FFmpeg 头文件**：
   ```bash
   grep -r "libavcodec\|libavformat\|libavutil\|libswscale" \
       --include="*.h" --include="*.cpp" \
       src/engine/ app/ src/oakgl/ src/oakcolor/
   ```

2. **非 oakgl 模块包含 OpenGL 头文件**：
   ```bash
   grep -r "#include <GL/\|#include <OpenGL/\|#include <EGL/" \
       --include="*.h" --include="*.cpp" \
       src/engine/ app/ src/oakcodec/ src/oakcolor/
   ```

3. **模块间直接链接**：
   ```cmake
   # 错误：oakengine 不应直接链接 oakcodec
   target_link_libraries(oakengine PRIVATE oakcodec)
   
   # 正确：oakengine 只链接 oakshared 和 Qt
   target_link_libraries(oakengine PRIVATE oakshared Qt6::Core)
   ```

4. **跨模块传递 C++ 对象**：
   ```cpp
   // 错误：将 std::shared_ptr<olive::Decoder> 传给 engine
   void bad_api(std::shared_ptr<olive::Decoder> dec);
   
   // 正确：只传递不透明句柄
   void good_api(OakDecoderHandle dec);
   ```

---

## 四、运行时加载器（Runtime Loader）

`oakengine.so` 内部通过三个运行时加载器与外部模块通信：

### 4.1 `OakCodecRuntime`

```cpp
class OakCodecRuntime : public OakRuntimeLoader {
public:
    static OakCodecRuntime* Instance();
    
    // 函数指针，在 Load() 中通过 dlsym 解析
    OakDecoderHandle (*decoder_open)(const char*, const char*, OakMediaInfo*);
    void (*decoder_close)(OakDecoderHandle);
    int (*decoder_read_video)(OakDecoderHandle, int, int64_t, int64_t, void*, OakFrame*);
    // ... 其他 codec API
};
```

### 4.2 `OakColorRuntime`

```cpp
class OakColorRuntime : public OakRuntimeLoader {
public:
    static OakColorRuntime* Instance();
    
    OakColorConfigHandle (*color_config_load)(const char*);
    void (*color_config_free)(OakColorConfigHandle);
    OakColorProcessorHandle (*color_processor_create)(...);
    // ... 其他 color API
};
```

### 4.3 `OakRendererRuntime`

```cpp
class OakRendererRuntime : public OakRuntimeLoader {
public:
    static OakRendererRuntime* Instance();
    
    OakRendererHandle (*renderer_create)(const char*, void*);
    void (*renderer_destroy)(OakRendererHandle);
    OakTextureHandle (*texture_upload)(...);
    // ... 其他 renderer API
};
```

**加载时机**：
- 懒加载：第一次调用 `Instance()->Load()` 时才 `dlopen` 目标 `.so`。
- 失败回退：若 `dlopen` 失败（如文件缺失），返回 `false`，调用方优雅降级（如禁用 GPU 功能）。
- 单例模式：每个 Runtime 是进程级单例，避免重复 `dlopen`。

---

## 五、数据流与零拷贝

### 5.1 解码 → 渲染 → 编码 全链路

```
文件输入
   │
   ▼
oakcodec.so (解码)
   │  ├─ 硬件加速路径：OAK_FRAME_EXTERNAL (CVPixelBuffer / D3D11 / VAAPI)
   │  └─ 软件路径：OAK_FRAME_GPU (YUV planar texture → RGBA32F shader 转换)
   │
   ▼
OakFrame (GPU 或 EXTERNAL)
   │
   ▼
oakengine.so (节点图渲染)
   │  ├─ 各节点通过 OakTextureHandle 操作帧
   │  └─ 色彩校正节点通过 OakColorProcessorHandle 调用 oakcolor.so
   │
   ▼
OakFrame (GPU)
   │
   ▼
oakgl.so (最终合成 / 显示)
   │  ├─ Viewer: oak_renderer_apply_display_transform (ACEScg → sRGB)
   │  └─ 导出: oak_renderer_readback_frame (GPU → CPU RGBA32F)
   │
   ▼
oakcodec.so (编码)
   │  ├─ 输入：RGBA32F + ACEScg (CPU 或 GPU)
   │  ├─ ODT：ACEScg → Output - Rec.709 (via oakcolor.so)
   │  └─ 格式转换：RGBA32F → YUV420P8 (via FFmpeg sws_scale)
   │
   ▼
文件输出
```

**零拷贝要点**：
1. 硬件解码的帧从始至终不离开 GPU / 硬件 surface。
2. 软件解码的 YUV 帧通过 `oak_texture_create_planar` 上传为 planar texture，再通过 GPU shader 转换为 RGBA32F。
3. 节点图内部所有纹理操作都在 GPU 上完成，只有最终编码或预览回读时才可能触发 CPU 内存拷贝。

---

## 六、模块替换示例

### 6.1 替换 FFmpeg 为自研解码器

1. 实现一个新的 `mydecoder.so`，导出与 `oakcodec.so` 完全相同的 C API。
2. 修改 `OakCodecRuntime::Load()` 的加载路径：
   ```cpp
   if (!OakRuntimeLoader::Load("libmydecoder.dylib")) {
       // 回退到 FFmpeg
       OakRuntimeLoader::Load("liboakcodec.dylib");
   }
   ```
3. `oakengine`、`app` 完全不需要重新编译。

### 6.2 添加 Vulkan 渲染后端

1. 实现 `oakvulkan.so`，导出与 `oakgl.so` 相同的 C API。
2. 在 `oak_renderer_create` 中根据 `backend_name` 选择加载哪个 `.so`：
   ```cpp
   if (strcmp(backend_name, "vulkan") == 0) {
       OakRendererRuntime::Instance()->Load("liboakvulkan.dylib");
   } else {
       OakRendererRuntime::Instance()->Load("liboakgl.dylib");
   }
   ```
3. `oakengine` 的 `OpenGLRendererProxy` 可以平滑替换为 `VulkanRendererProxy`，上层节点图无感知。

---

## 七、测试策略

### 7.1 模块级黑盒测试

每个模块有独立的 gtest 文件，直接 `dlopen` 目标 `.so`：

| 测试文件 | 目标模块 | 测试重点 |
|----------|---------|----------|
| `c_api_codec_test.cpp` | `oakcodec.so` | 解码器生命周期、帧分配、格式转换 |
| `c_api_color_test.cpp` | `oakcolor.so` | 配置加载、处理器创建、LUT 应用 |
| `c_api_engine_test.cpp` | `oakengine.so` | 项目加载、会话创建、渲染帧 |
| `c_api_renderer_test.cpp` | `oakgl.so` | 纹理上传/下载、目标创建、着色器编译 |
| `c_api_integration_test.cpp` | 多模块 | 跨模块数据流、色彩转换后帧处理 |

### 7.2 CI 检测脚本

```bash
#!/bin/bash
# 检测违规的跨模块依赖

ERR=0

# 1. 非 oakcodec 包含 FFmpeg
if grep -r "libavcodec\|libavformat\|libavutil\|libswscale\|libswresample" \
    --include="*.h" --include="*.cpp" \
    src/engine/ app/ src/oakgl/ src/oakcolor/; then
    echo "ERROR: FFmpeg headers found outside oakcodec"
    ERR=1
fi

# 2. 非 oakgl 包含 OpenGL
if grep -r "#include <GL/\|#include <OpenGL/\|#include <EGL/" \
    --include="*.h" --include="*.cpp" \
    src/engine/ app/ src/oakcodec/ src/oakcolor/; then
    echo "ERROR: OpenGL headers found outside oakgl"
    ERR=1
fi

# 3. 模块间直接链接
if grep -r "target_link_libraries.*oakengine.*oakcodec\|target_link_libraries.*oakcodec.*oakgl" \
    --include="CMakeLists.txt" src/ app/; then
    echo "ERROR: Direct inter-module linking detected"
    ERR=1
fi

exit $ERR
```

---

## 八、总结

Oak 的模块架构遵循以下核心原则：

1. **严格分层**：`oakshared` → 独立 `.so` 模块 → `app`。下层不依赖上层，同层不直接链接。
2. **C API 契约**：所有跨模块边界都是 `extern "C"` 函数 + 不透明句柄 + POD 结构。
3. **运行时解耦**：优先使用 `dlopen` + `dlsym`，避免编译时链接导致的符号污染和版本锁定。
4. **零拷贝数据流**：通过 `OakFrame` 描述符在 CPU/GPU/EXTERNAL 三种存储模式间传递，减少不必要的内存拷贝。
5. **可替换性**：任何模块都可以在保持 C API 不变的前提下被重写或替换，上层无感知。

这套架构的代价是多了一层函数指针调用和 `dlopen` 开销，但在现代桌面系统中这部分开销可以忽略不计（微秒级），换来的是清晰的边界、独立的演化能力和强大的可测试性。
