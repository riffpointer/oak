# Oak 模块化拆分与多进程渲染计划（v2 — GPU 零拷贝版）

> 目标：将 monolithic 的 engine 拆分为多个可独立替换的动态库，最终支持用 Rust 逐个替换模块。  
> v2 核心变化：**全局 GPU 零拷贝** + **FFmpeg 完全封装**。  
> 新增跨模块帧抽象 `OakFrame`，`oakcodec.so` 运行时加载 `oakgl.so` 完成 upload/conversion，外部永不触碰 FFmpeg。

---

## 一、核心原则（v2 更新）

1. **模块间只允许纯 C 接口**：内部可以用 C++ / Qt，但跨模块调用必须通过 `extern "C"` 的函数指针和 `void*` handle。
2. **最大限度地剥离 Qt**：按 `shared` → `codec` → `audio` → `color` → `node` 的顺序，把 Qt 数据结构替换为 STL。
3. **渲染器必须是独立进程**：每个渲染器进程只渲染约 100 帧，用完即弃，通过 stdio + 共享内存与主进程通信。
4. **OpenGL 必须被抽象层完全封装**：外部代码不再直接包含任何 OpenGL 头文件，只调用与底层图形 API 无关的纯 C 绘制接口。
5. **FFmpeg 完全封装**（v2 新增）：
   - **唯一合法入口**：只有 `oakcodec.so` 内部可以 `#include <libavcodec/avcodec.h>` 等 FFmpeg 头文件。
   - **外部绝对禁止**：`oakengine.so`、`oaknodes.so`、`oakgl.so`、`oakrenderer` 进程不得直接包含或使用任何 FFmpeg 类型。
   - **检测手段**：CI 扫描非 `src/oakcodec/` 目录下的 FFmpeg 引用，违规即构建失败。
6. **全局 GPU 零拷贝**（v2 新增）：
   - 视频帧从解码到渲染应尽量留在 GPU，不经过 CPU swscale / upload。
   - `oakcodec.so` 在内部通过 `dlopen/dlsym` 加载 `oakgl.so`，完成 YUV planar upload、YUV→RGBA shader、外部 surface 包装。
   - 仅在必要时（缩略图、软件编码器输入、特效节点需要 CPU 处理）才回读到 CPU。
7. **全链路 ACEScg 工作空间**（v2 新增）：
   - **内部统一工作空间**：`ACES - ACEScg`（AP1 primaries, scene-referred linear）。
   - **输入端 IDT**：`oakcodec.so` 解码器通过 `oakcolor.so` 将源文件色彩空间（Rec.709、ARRI LogC、Apple Log 等）转换到 ACEScg。
   - **输出端 ODT**：`oakcodec.so` 编码器通过 `oakcolor.so` 将 ACEScg 转换到目标编码空间（Rec.709、Rec.2020 PQ、P3-DCI 等）。
   - **显示端 View Transform**：`oakengine.so` 预览窗口通过 `oakcolor.so` 的 Display Transform（RRT + ODT）将 ACEScg 转换到显示设备色彩空间。
   - **节点图内部**：所有节点（Blur、Merge、ColorCorrection 等）在 ACEScg linear 下工作。若 `PluginNode` 调用的 OFX 插件不支持 ACEScg，输入/输出端口自动做隐式转换。
   - **禁止直接显示 ACEScg**：ACEScg 是 scene-referred linear，直接显示会发灰。必须通过 View Transform。

---

## 二、模块划分

| 模块名 | 内部内容 | 对外 C API | 内部 Qt | 运行时加载关系 |
|--------|----------|-----------|---------|----------------|
| `oakcore.so` | 节点图基础设施：`Node` 基类、`NodeGraph`、连接关系、序列化、`NodeValue` 类型系统 | `oak_node_api.h` | ✅ 允许（P5 再剥离） | — |
| `oaknodes.so` | 所有具体节点实现：音频、视频、颜色、特效、生成器、过渡、OFX `PluginNode` | 通过 `oak_node_api.h` 的**注册机制**注入到 core | ✅ 允许 | — |
| `oakcodec.so` | FFmpeg/OIIO 解码器、编码器、conform、planar file | `oak_codec_api.h` + `oak_frame_api.h` | ✅ 允许 | **运行时加载 `oakgl.so`** |
| `oakcolor.so` | OCIO `ColorManager`、色彩处理器、LUT 管理 | `oak_color_api.h` | ✅ 允许 | — |
| `oakaudio.so` | 音频混合、重采样、格式转换、`AudioManager` | `oak_audio_api.h` | ✅ 允许 | — |
| `oakgl.so` | **OpenGL 渲染后端**（唯一知道 OpenGL 的模块） | `oak_renderer_api.h` + `oak_frame_api.h` | ✅ 允许 | — |
| `oakpluginhost.so` | OpenFX HostSupport、参数实例、效果实例 | OpenFX 标准 C API + 宿主注册 C API | ✅ 允许 | — |
| `oakrenderer` | **独立可执行文件**：节点图求值、帧调度、调用渲染后端 | stdio JSON + 共享内存 | ❌ **零 Qt** | 链接 `oakcore` + `oaknodes` + `oakgl` |
| `oakengine.so` | 主进程协调层：`RenderManager`、`PreviewAutoCacher`、`DiskManager`、任务队列 | `oak_coord_api.h` | ✅ 允许 | 链接 `oakcodec` + `oakaudio` + `oakcolor` |

---

## 三、关键技术设计（v2 更新）

### 3.1 跨模块帧抽象：`OakFrame`（v2 新增）

`OakFrame` 是 `oakcodec.so`、`oakgl.so`、`oakengine.so` 之间的**共享帧描述符**：

```c
/* oak_frame_api.h — 既不依赖 FFmpeg，也不依赖 OpenGL */
typedef struct OakFrame {
    int width, height;
    OakFramePixelFormat pix_fmt;   /* RGBA8 / YUV420P8 / NV12 / HW_VIDEOTOOLBOX ... */
    OakFrameStorage     storage;   /* CPU / GPU / EXTERNAL */
    int64_t pts_num, pts_den;
    void* data[4];     /* CPU buffer 或 OakTextureHandle 或平台句柄 */
    int   stride[4];
    int   planes;
    void* internal;    /* codec 内部 AVFrame，外部禁止读取 */
} OakFrame;

void oak_frame_release(OakFrame* frame);
```

**三种存储模式**：

| 模式 | 说明 | 释放方式 |
|------|------|----------|
| `OAK_FRAME_CPU` | `data[0]` 指向 CPU buffer，codec 内部通过 `swscale` 生成 | `oak_frame_release` 释放 AVFrame + buffer |
| `OAK_FRAME_GPU` | `data[0..planes-1]` 为 `OakTextureHandle`，由 codec 内部调用 `oakgl` upload | `oak_frame_release` 调用 `oak_texture_destroy` |
| `OAK_FRAME_EXTERNAL` | `data[0]` 为平台句柄（如 `CVPixelBufferRef`），codec 通过 `oak_texture_wrap_external` 包装 | `oak_frame_release` 仅释放 AVFrame 引用，不销毁底层 surface |

### 3.2 零拷贝解码流程（含 ACEScg IDT）

```
oak_decoder_read_video(decoder, stream, time, renderer_hint, out_frame)
│
├─ 解码原始帧（FFmpeg）
│   └─ 硬件加速? → GPU surface (CVPixelBuffer / D3D11 / VAAPI)
│   └─ 软件解码? → CPU YUV buffer
│
├─ 色彩空间转换（IDT）
│   └─ 通过文件 metadata / 用户指定 / 启发式探测源色彩空间
│   └─ 调用 oakcolor.so: src_space → "ACES - ACEScg"
│   └─ out_frame->colorspace = "ACES - ACEScg"
│
├─ renderer_hint == NULL ?
│   └─ YES → swscale YUV→RGBA32F ACEScg → CPU buffer → OAK_FRAME_CPU
│
└─ renderer_hint != NULL ?
    ├─ 硬件解码器可用?
    │   └─ YES → oak_texture_wrap_external(GPU surface) → OAK_FRAME_EXTERNAL
    │           (色彩空间已通过 IDT 转换到 ACEScg，GPU surface 语义为 ACEScg)
    │
    └─ 软件解码 YUV CPU buffer
        ├─ 默认路径（推荐）
        │   └─ upload YUV planar → GPU texture
        │   └─ shader YUV→RGBA32F ACEScg → OAK_FRAME_GPU (data[0] = RGBA texture)
        │
        └─ 特殊路径（caller 要求保留 YUV）
            └─ upload YUV planar → GPU texture → OAK_FRAME_GPU (data[0..2] = Y/U/V textures)
```

**回退策略**：若 `dlopen("liboakgl.dylib")` 失败，codec 静默回退到 CPU 路径，但仍做 IDT 转换到 ACEScg。

### 3.3 oakgl.so 扩展接口（v2 新增）

| 函数 | 作用 |
|------|------|
| `oak_texture_create_planar` | 上传 YUV planar CPU buffer 为多张 GPU texture（语义色彩空间为 ACEScg） |
| `oak_texture_wrap_external` | 包装平台硬件 surface（CVPixelBuffer / D3D11Texture / VASurface），语义为 ACEScg |
| `oak_renderer_blit_yuv_to_rgba` | GPU shader YUV→RGBA32F ACEScg 转换 |
| `oak_renderer_readback_frame` | GPU texture/target 回读到 `OakFrame`（CPU，RGBA32F + ACEScg） |

### 3.4 编码器路径（含 ACEScg ODT）

```c
/* 编码器输入改为 OakFrame*，期望 RGBA32F + ACEScg */
int oak_encoder_write_video(OakEncoderHandle enc, const OakFrame* frame);
```

**编码流程**：
1. **确认输入**：`frame` 应为 RGBA32F + `"ACES - ACEScg"`。
   - 若不是，先做隐式转换（format + colorspace）。
2. **ODT 转换**：通过 `oakcolor.so` 将 ACEScg → `output_colorspace`（如 Rec.2020 PQ）。
3. **格式转换**：通过 `sws_scale` 或 GPU shader 将 RGBA32F → `output_pix_fmt`（如 YUV420P10）。
4. **送入编码器**：FFmpeg 硬件/软件编码器写入文件。

| 存储模式 | 处理路径 |
|----------|----------|
| `OAK_FRAME_CPU` | 直接读取 `data[0]`（RGBA32F ACEScg）→ ODT → 格式转换 → 编码。 |
| `OAK_FRAME_GPU` | `oak_renderer_readback_frame` 回读到 CPU（RGBA32F ACEScg）→ ODT → 编码。 |
| `OAK_FRAME_EXTERNAL` | 若编码器支持硬件编码（VideoToolbox），直接提交 surface，跳过 CPU 回读。但仍需 ODT（若硬件编码器不支持色彩空间转换，则先 readback 再做 ODT）。 |

### 3.5 节点系统的 C API 转发模式（v2 更新：色彩空间）

```c
// 节点求值返回的值类型扩展为支持 OakFrame
int   oak_value_get_type(OakValueHandle v);  // OAK_VALUE_FRAME / AUDIO / INT / FLOAT ...

// 提取帧（返回浅拷贝的 OakFrame，不转移所有权）
void  oak_value_get_frame(OakValueHandle v, OakFrame* out_frame);

// 提取音频（保持不变）
void* oak_value_get_audio_data(OakValueHandle v, int* channels, int64_t* samples);
```

**节点图色彩空间约定**：
- 所有节点的输入/输出端口默认契约：**RGBA32F + ACEScg**。
- `PluginNode`（OFX 插件包装）的特殊处理：
  - 若插件声明支持 `kOfxBitDepthFloat` 且宿主传递 ACEScg metadata：直接零拷贝。
  - 若插件不支持 ACEScg（如旧版 8-bit 插件）：
    - **输入端口**：通过 `oakcolor.so` 做 ACEScg → sRGB（或插件期望空间），同时像素格式 RGBA32F → RGBA8。
    - **插件内部**：插件按自己的空间工作。
    - **输出端口**：通过 `oakcolor.so` 做 sRGB → ACEScg，像素格式 RGBA8 → RGBA32F。
  - 这些转换对 caller 是透明的，`PluginNode` 内部自动插入。

### 3.6 OpenGL 渲染后端的 C API 抽象

保持不变，详见 `renderer_api.md`。v2 重点扩展了纹理管理和 YUV 转换能力。

---

## 四、实施路线图（v2 更新）

| 阶段 | 目标 | 产出 | 复杂度 |
|------|------|------|--------|
| **P0** | 建立 C API 骨架 + 品牌统一 | `include/oak/{frame,node,codec,color,audio,renderer}_api.h` | 低 |
| **P1** | `oakgl.so` 独立 | 把 OpenGL 包装为动态库，对外暴露 `oak_renderer_api.h` | 中 |
| **P2** | `oakcodec.so` 独立（v1） | 消除 `codec/` 对 `node/` 的 C++ 依赖，编译为独立 `.so` | 中 |
| **P2.5** | **GPU 零拷贝 + FFmpeg 封装 + ACEScg 全链路**（v2） | 新增 `oak_frame_api.h`；扩展 `oak_renderer_api.h`；重写 `oak_codec_api.h`；更新 `oak_color_api.h`；codec 运行时加载 oakgl；引入 ACEScg 工作空间 | **高** |
| **P3** | `oakaudio.so` 独立 | 提取音频处理核心，消除对节点图的直接依赖 | 低 |
| **P4** | `oakcore.so` + `oaknodes.so` 分离 | 把节点基础设施与具体节点实现拆成两个 `.so`，用注册机制连接 | **高** |
| **P5** | `oakrenderer` 可执行文件 | 基于 `oakcore` C API + `oakgl` C API，独立进程，stdio + 共享内存 | **高** |
| **P6** | 逐步 Qt 剥离 | 按 `shared/` → `codec/` → `audio/` → `color/` → `node/` 顺序替换数据结构 | **高** |

**P2.5 是 v2 新增阶段**，原因在于：
- 接口变更影响面大（codec / gl / engine / nodes 都需要同步）
- 需要验证硬件加速路径（VideoToolbox、VAAPI、D3D11）
- 需要编写 CI 扫描脚本确保 FFmpeg 封装边界不被破坏

---

## 五、FFmpeg 封装边界检测脚本（v2 新增）

在 `.github/workflows/ci.yml` 中增加以下步骤：

```yaml
- name: Check FFmpeg encapsulation boundary
  run: |
    # 只有 src/oakcodec/ 允许包含 FFmpeg 头文件
    if grep -rE "#include\s+<lib(avcodec|avformat|avutil|swscale|swresample|avfilter)" \
       --include="*.h" --include="*.cpp" \
       engine/src/ app/src/ src/oakgl/ src/oakengine/ src/oakaudio/ src/oakcolor/ src/oakcore/ src/oaknodes/; then
      echo "ERROR: FFmpeg headers found outside src/oakcodec/"
      exit 1
    fi
    echo "FFmpeg encapsulation OK"
```

---

## 六、关于 Rust 替换

这套架构下，替换任何模块为 Rust 的步骤不变：

1. **保持 C API 头文件不变**（这是契约）。
2. **用 Rust 实现同样的 C ABI 导出函数**（`#[no_mangle] pub extern "C"`）。
3. **主进程通过 `dlopen` 加载新的 `.so`/`.dylib`**，替换掉旧的 C++ 实现。

**Rust 替换 `oakcodec.so` 的注意事项**：
- Rust 侧需要链接 `ffmpeg-next` crate 处理解码/编码。
- Rust 侧同样需要 `dlopen` 加载 `oakgl.so`，因为 Rust 不能编译时依赖 C++ OpenGL 代码。
- `OakFrame::internal` 在 Rust 侧可以是一个 `*mut c_void`，实际指向 Rust 管理的 `AVFrame` 包装结构。
