# Oak 模块化拆分与多进程渲染计划

> 目标：将 monolithic 的 engine 拆分为多个可独立替换的动态库，最终支持用 Rust 逐个替换模块。

## 一、核心原则

1. **模块间只允许纯 C 接口**：内部可以用 C++ / Qt，但跨模块调用必须通过 `extern "C"` 的函数指针和 `void*` handle。
2. **最大限度地剥离 Qt**：按 `shared` → `codec` → `audio` → `color` → `node` 的顺序，把 Qt 数据结构替换为 STL。
3. **渲染器必须是独立进程**：每个渲染器进程只渲染约 100 帧，用完即弃，通过 stdio + 共享内存与主进程通信。
4. **OpenGL 必须被抽象层完全封装**：外部代码不再直接包含任何 OpenGL 头文件，只调用与底层图形 API 无关的纯 C 绘制接口。

## 二、模块划分

| 模块名 | 内部内容 | 对外 C API | 内部 Qt |
|--------|----------|-----------|---------|
| `oakcore.so` | 节点图基础设施：`Node` 基类、`NodeGraph`、连接关系、序列化、`NodeValue` 类型系统 | `oak_node_api.h` | ✅ 允许（P5 再剥离） |
| `oaknodes.so` | 所有具体节点实现：音频、视频、颜色、特效、生成器、过渡、OFX `PluginNode` | 通过 `oak_node_api.h` 的**注册机制**注入到 core | ✅ 允许 |
| `oakcodec.so` | FFmpeg/OIIO 解码器、编码器、conform、planar file | `oak_codec_api.h` | ✅ 允许 |
| `oakcolor.so` | OCIO `ColorManager`、色彩处理器、LUT 管理 | `oak_color_api.h` | ✅ 允许 |
| `oakaudio.so` | 音频混合、重采样、格式转换、`AudioManager` | `oak_audio_api.h` | ✅ 允许 |
| `oakgl.so` | **OpenGL 渲染后端**（唯一知道 OpenGL 的模块） | `oak_renderer_api.h`（与 OpenGL 无关的绘制抽象） | ✅ 允许 |
| `oakpluginhost.so` | OpenFX HostSupport、参数实例、效果实例 | OpenFX 标准 C API + 宿主注册 C API | ✅ 允许 |
| `oakrenderer` | **独立可执行文件**：节点图求值、帧调度、调用渲染后端 | stdio JSON + 共享内存 | ❌ **零 Qt** |
| `oakengine.so` | 主进程协调层：`RenderManager`、`PreviewAutoCacher`、`DiskManager`、任务队列 | `oak_coord_api.h` | ✅ 允许 |

## 三、关键技术设计

### 3.1 节点系统的 C API 转发模式

所有节点继承自 `Node`，正好适合 **handle + dynamic_cast 转发**：

```c
// oak_node_api.h
typedef struct OakNode* OakNodeHandle;       // 实际是 Node*
typedef struct OakGraph* OakGraphHandle;     // 实际是 NodeGraph*

// 工厂：oaknodes.so 在加载时注册自己
typedef OakNodeHandle (*OakNodeFactoryFn)(void);
void oak_node_register_type(const char* id, OakNodeFactoryFn factory);

// 创建/销毁
OakNodeHandle oak_node_create(const char* type_id);
void          oak_node_destroy(OakNodeHandle n);

// 通用属性（内部 dynamic_cast<Node*> 后调虚函数）
void        oak_node_set_label(OakNodeHandle n, const char* label);
const char* oak_node_get_label(OakNodeHandle n);

// 参数读写（通过 Node::SetParam / Node::GetParam 虚函数转发）
void oak_node_set_param_int(OakNodeHandle n, const char* param, int64_t val);
void oak_node_set_param_double(OakNodeHandle n, const char* param, double val);
void oak_node_set_param_rational(OakNodeHandle n, const char* param, int64_t num, int64_t den);

// 连接（内部操作 NodeInput/NodeOutput）
void oak_node_connect(OakNodeHandle out_node, const char* output_id,
                      OakNodeHandle in_node,  const char* input_id);

// 求值（返回类型擦除的值）
typedef struct OakValue* OakValueHandle;
OakValueHandle oak_node_value_at_time(OakNodeHandle n, const char* output_id, 
                                      int64_t num, int64_t den);
void oak_value_destroy(OakValueHandle v);

// 值的类型识别与提取
int   oak_value_get_type(OakValueHandle v);  // OAK_VALUE_FRAME / AUDIO / INT / FLOAT ...
void* oak_value_get_frame_data(OakValueHandle v, int* w, int* h, int* pix_fmt);
void* oak_value_get_audio_data(OakValueHandle v, int* channels, int64_t* samples);
```

**关键点**：
- `oakcore.so` 只持有 `Node*` 基类指针，通过虚函数调度。
- `oaknodes.so` 在 `dlopen` 时调用 `oak_node_register_type("blur", &create_blur)` 注入工厂。
- `oakrenderer` 进程链接 `oakcore.so` + `oaknodes.so`，通过同样的 C API 求值，但跑在独立进程里。
- 未来把 `oaknodes.so` 替换成 Rust 时，Rust 侧只需实现同样的 `oak_node_register_type` + C ABI 工厂函数即可。

### 3.2 OpenGL 渲染后端的 C API 抽象

把 OpenGL 完全关进 `oakgl.so`，对外暴露与 GL 无关的绘制接口：

```c
// oak_renderer_api.h
typedef struct OakRenderer* OakRendererHandle;
typedef struct OakTexture*  OakTextureHandle;
typedef struct OakTarget*   OakTargetHandle;

// 加载后端（内部 dlopen("oakgl.so") 或 "oakvk.so"）
OakRendererHandle oak_renderer_create(const char* backend_name);
void              oak_renderer_destroy(OakRendererHandle r);

// 资源管理
OakTextureHandle oak_renderer_upload_texture(OakRendererHandle r,
                                             int w, int h, int pix_fmt,
                                             const void* data, size_t data_size);
void             oak_renderer_destroy_texture(OakRendererHandle r, OakTextureHandle t);

OakTargetHandle oak_renderer_create_target(OakRendererHandle r, int w, int h);
void            oak_renderer_destroy_target(OakRendererHandle r, OakTargetHandle t);

// 高级绘制指令（与 OpenGL 完全无关）
void oak_renderer_begin(OakRendererHandle r, OakTargetHandle target, 
                        int w, int h, const float* clear_color_rgba);

void oak_renderer_draw_quad(OakRendererHandle r,
                            const float* mvp_matrix_4x4,
                            OakTextureHandle tex,
                            int blend_mode,
                            const float* uv_coords);

void oak_renderer_draw_text(OakRendererHandle r,
                            const char* utf8_text,
                            const float* transform_4x4,
                            int font_size,
                            const float* color_rgba);

void oak_renderer_end(OakRendererHandle r);

// 回读结果（供共享内存传输）
int oak_renderer_readback(OakRendererHandle r, OakTargetHandle target,
                          void* out_buffer, size_t buffer_size);
```

**替换路径**：
- 以后写 `oakvk.so`（Vulkan 后端），对外暴露同样的 `oak_renderer_api.h` 符号。
- 甚至可以写 `oak_wgpu.so`（Rust + wgpu），只要导出同样的 C 函数。
- `oakrenderer` 进程完全不感知底层是 OpenGL 还是 Vulkan。

### 3.3 编解码独立

当前 `codec/` 已经物理隔离在 `engine/src/codec/`。需要消除的 C++ 交叉依赖：

| 当前依赖 | 解决方案 |
|----------|----------|
| `decoder.h` 包含 `node/block/block.h` | 把 `Block` 中涉及解码的纯数据结构（如时间范围）提取到 `shared/` |
| `decoder.h` 包含 `node/project/footage/footagedescription.h` | `FootageDescription` 是纯 POD，移到 `shared/include/oak/codec/footagedescription.h` |

最终 `oakcodec.so` 只依赖 `shared/`（基础类型）和 FFmpeg、OIIO 外部库。

### 3.4 音频独立

`oakaudio.so` 包含音频混合算法、重采样、格式转换、`AudioManager`。

```c
// oak_audio_api.h
typedef struct OakAudioMixer* OakAudioMixerHandle;
OakAudioMixerHandle oak_audio_mixer_create(int channels, int64_t sample_rate);
void oak_audio_mixer_add_source(OakAudioMixerHandle m, const float* interleaved, int64_t samples);
void oak_audio_mixer_mix(OakAudioMixerHandle m, float* out, int64_t out_samples);
void oak_audio_mixer_destroy(OakAudioMixerHandle m);
```

## 四、实施路线图

| 阶段 | 目标 | 产出 | 复杂度 |
|------|------|------|--------|
| **P0** | 建立 C API 骨架 + 品牌统一 | `engine/include/oak/{node,codec,color,audio,renderer}_api.h`；所有 `olive` 前缀改为 `oak` | 低 |
| **P1** | `oakgl.so` 独立 | 把 `render/opengl/` 包装为动态库，对外暴露 `oak_renderer_api.h`，外部不再直接 `#include <QOpenGL>` | 中 |
| **P2** | `oakcodec.so` 独立 | 消除 `codec/` 对 `node/` 的 C++ 依赖，编译为独立 `.so` | 中 |
| **P3** | `oakaudio.so` 独立 | 提取音频处理核心，消除对节点图的直接依赖 | 低 |
| **P4** | `oakcore.so` + `oaknodes.so` 分离 | 把节点基础设施与具体节点实现拆成两个 `.so`，用注册机制连接 | **高** |
| **P5** | `oakrenderer` 可执行文件 | 基于 `oakcore` C API + `oakgl` C API，独立进程，stdio + 共享内存 | **高** |
| **P6** | 逐步 Qt 剥离 | 按 `shared/` → `codec/` → `audio/` → `color/` → `node/` 顺序替换数据结构 | **高** |

## 五、关于 Rust 替换

这套架构下，替换任何模块为 Rust 的步骤是：

1. **保持 C API 头文件不变**（这是契约）。
2. **用 Rust 实现同样的 C ABI 导出函数**（`#[no_mangle] pub extern "C"`）。
3. **主进程通过 `dlopen` 加载新的 `.so`/`.dylib`**，替换掉旧的 C++ 实现。

例如替换 `oakgl.so` 为 Rust + wgpu：

```rust
#[no_mangle]
pub extern "C" fn oak_renderer_create(backend: *const c_char) -> *mut OakRenderer {
    Box::into_raw(Box::new(WgpuRenderer::new()))
}
```
