# Oak Video Editor 测试策略与计划

本文档描述 Oak Video Editor 的自动化测试策略，包括单元测试、集成测试、动态库覆盖率现状与补齐计划，以及 CI 执行方式。

## 目标

- 尽量自动化，减少人工测试。
- 覆盖所有模块（至少一个自动化测试）。
- 集成测试保持无 GUI（headless）。
- 在 Windows/macOS/Linux 上可重复运行。
- **各动态库 C API 函数级覆盖率接近 100%，核心路径零跳过。**

## 测试层级

### 1) 单元测试（GoogleTest）
- 目标：小范围、确定性、无 GUI。
- 目录：`tests/gtest/`。
- 执行：`ctest` 里的 `olive-gtest`。

### 1.5) 模块冒烟测试（GoogleTest）
- 目标：对 GUI 相关模块做编译期/链接期覆盖，不实例化控件。
- 目录：`tests/gtest/module_smoke_test.cpp`。
- 执行：`ctest` 里的 `olive-gtest`。

### 2) 集成测试（GoogleTest）
- 目标：跨模块流程但不依赖 GUI（例如序列化→反序列化）。
- 目录：`tests/gtest/`（如 `ProjectSerializer`、`TaskManager`）。

### 3) 现有测试（Olive 宏测试）
- 目录：`tests/general`、`tests/timeline`、`tests/compositing` 保持不变。

---

## 动态库覆盖率现状评估（2026-05-27）

本项目共有 **9 个动态库（.so/.dylib/.dll）**。经对 `tests/gtest/` 现有测试代码与 `include/oak/*.h` 公开 API 逐一比对，覆盖率如下：

| 动态库 | 头文件 | 函数数（约） | 当前覆盖率 | 测试文件 | 关键缺口 |
|---|---|---|---|---|---|
| **oakaudio** | `audio_api.h` | 16 | **0%** | 无 | 全部未测：buffer/clone、resampler、mixer、filter graph、convert_layout |
| **oakcodec** | `codec_api.h` | ~45 | **~30%** | `c_api_codec_test.cpp` | decoder_open（无真实文件）、read_video、read_audio、thumbnail、read_video_ex、conform 全链路、encoder 参数设置与写入、frame_release、被跳过的 format 映射测试 |
| **oakcolor** | `color_api.h` | ~45 | **~50%** | `c_api_color_test.cpp` | config_get_space、display_view_count/name、processor_create_from_lut（stub）、processor_create_transform/display、display_transform_apply、gpu_shader LUT/texture 元数据查询、grading_primary 全部 setter（仅 saturation 测了）、processor_create_from_grading、color_space_equal |
| **oakgl** | `renderer_api.h` | ~40 | **~45%** | `c_api_renderer_test.cpp` | texture_upload_from_frame、create_planar、wrap_external（stub）、renderer_get_pixel、target_detach_color_texture、clear_texture、draw_text（stub）、draw_lines、draw_polygon、apply_effect、apply_display_transform、blit_yuv_to_rgba、readback_frame（stub）、draw_with_shader 全系（JSON + binary v2）、font_load/destroy |
| **oakengine** | `engine_api.h` | 6 | **~50%** | `c_api_engine_test.cpp` | 函数均有测试，但 `session_create` / `render_frame` 在无显示环境（headless CI）下会 GTEST_SKIP，导致核心渲染路径在 CI 中实际未执行 |
| **oakpluginhost** | `pluginhost_api.h` | 30+ | **0%** | 无 | host 生命周期、插件加载、实例化、参数交互、时间线回调 |

> **注**：`oakcore`、`oaknodes`、`oakcoord` 已合并进 `oakengine`，不再作为独立动态库存在，其 C API 由 engine 内部统一承载。以下计划仅针对现存动态库。

> **结论**：当前没有任何一个动态库达到接近 100% 的函数级覆盖，其中 **oakaudio、oakpluginhost 完全零测试**。已写测试中也存在大量 `GTEST_SKIP`（如 oakcodec 的 4 个 format 映射测试、oakengine 的 session/render 测试、oakgl 的全部 GPU 测试），导致 CI 中实际执行的有效测试进一步减少。

---

## C API 全覆盖补齐计划

### 阶段一：P0 — 补全无测试的动态库与修复跳过项

**目标**：让 CI 中不再有任何永久跳过的测试；零测试库至少具备基础生命周期 + 核心功能覆盖。

#### 1.1 新建 `c_api_audio_test.cpp`（oakaudio.so）

| 测试组 | 用例 | 验证点 |
|---|---|---|
| Buffer 生命周期 | `BufferCreateFree`、`BufferCreateZeroChannels`、`BufferClone` | create 返回非空；零通道返回空或优雅失败；clone 后参数一致、数据独立 |
| Buffer 数据访问 | `BufferParams`、`BufferDataInterleaved`、`BufferDataPlanar` | params 与创建参数一致；data 非空；interleaved 标志正确 |
| Resampler | `ResamplerCreateFree`、`Resampler48kTo44k1`、`ResamplerStereoToMono` | 创建成功；重采样后样本数合理；单声道输出非零 |
| Mixer | `MixerCreateFree`、`MixerAddSource`、`MixerClearSources`、`MixerMixSingle`、`MixerMixMulti` | 空 mixer mix 返回 0 或静音；单源混音音量正确；多源叠加正确；pan 左右声道有差异 |
| Filter Graph | `FilterGraphCreateFree`、`FilterGraphTempo`、`FilterGraphFlush` | atempo 2x 后输出长度减半；flush 输出残余样本；format 转换后声道/采样率正确 |
| Convert Layout | `ConvertLayoutII`、`ConvertLayoutIP`、`ConvertLayoutPI`、`ConvertLayoutPP` | interleaved↔planar 四种组合，数据一致性校验 |

#### 1.2 修复 oakcodec 中永久跳过的测试

现有 4 个 `GTEST_SKIP()`（`VideoFormatToAv*`、`AvToVideoFormat*`、`VideoFormatIsPlanar`）因“static init issue in headless env”被跳过。

- **根因**：`oak_video_format_to_av` 等函数依赖 FFmpeg 全局静态注册表，在 headless CI 中若 FFmpeg 未初始化可能导致崩溃或返回异常值。
- **修复方案**：
  1. 在 `tests/gtest/main.cpp` 或 fixture 的 `SetUpTestSuite` 中显式调用 `av_register_all()` / `avcodec_register_all()`（或等价的 FFmpeg 初始化）。
  2. 若 FFmpeg 版本已废弃全局注册（FFmpeg 4.0+），则检查是否有其他静态初始化依赖（如 `av_pix_fmt_desc_get` 的查找表），必要时在 `oakcodec` 内部做惰性初始化保护。
  3. 修复后删除 `GTEST_SKIP()`，改为正常断言。

#### 1.3 新建 `c_api_pluginhost_test.cpp`（oakpluginhost.so） stub 安全测试

该模块大量函数为 stub（TODO），测试以“不崩溃 + 返回预期错误码”为主。

| 测试组 | 用例 | 验证点 |
|---|---|---|
| Host 生命周期 | `HostCreateDestroy`、`HostSetCapability` | create 非空；destroy null 不崩溃 |
| 插件加载 | `LoadBundleMissing`、`LoadFromPathMissing` | 路径不存在时返回错误；不崩溃 |
| 实例化 | `InstanceCreateNullPlugin` | null plugin → null instance |
| 时间线回调 | `SetTimelineCallbacksNull` | null 输入不崩溃 |

---

### 阶段二：P1 — 扩展已有测试的核心路径

#### 2.1 `c_api_codec_test.cpp` 扩展

| 新增测试组 | 用例 | 验证点 |
|---|---|---|
| Decoder 真实文件 | `DecoderOpenValidMp4`、`DecoderProbeFile`、`DecoderOpenStream` | 使用 `tests/assets/` 中的真实 MP4；probe 返回的宽高/帧率正确；open_stream 后 is_open=1 |
| Video 解码 | `ReadVideoFrame0`、`ReadVideoFrameMid`、`ReadVideoFrameEof` | 帧宽高正确；pix_fmt 为 RGBA32F；data[0] 非空；EOF 返回非零或最后帧 |
| Thumbnail | `Thumbnail256`、`ThumbnailZeroSize` | max_size=256 时返回尺寸 ≤256；max_size=0 返回错误 |
| Extended decode | `ReadVideoExDivider`、`ReadVideoExMaxFormat` | divider=2 时尺寸减半；maximum_format=U8 时返回 RGBA8 |
| Audio 解码 | `ReadAudio1024`、`ReadAudioAcrossEof`、`ReadAudioNegativeStart` | 返回样本数 ≥0；EOF 时 < 请求数；负起始返回错误 |
| Conform | `ConformAudio48k`、`ConformPoll`、`ConformGetWait` | conform 后缓存目录出现文件；poll 返回 0~100；wait=true 阻塞到完成 |
| Encoder 完整链路 | `EncoderSetVideoParams`、`EncoderWrite10Frames`、`EncoderFinalizeValid` | 设置参数后写入 10 帧 RGBA32F；finalize 后文件可被 decoder_open 重新打开并读取；宽高匹配 |
| Frame 释放 | `FrameRelease`、`FrameReleaseInternalOnly` | release 后 data[0] 被清零；internal_only 只释放 internal |

**依赖**：需要准备 `tests/assets/c_api/test_10frames_1920x1080_h264.mp4`（< 500 KB，可用 FFmpeg 生成）。

#### 2.2 `c_api_color_test.cpp` 扩展

| 新增测试组 | 用例 | 验证点 |
|---|---|---|
| Config 查询 | `ConfigGetSpace`、`ConfigDisplayViewCount`、`ConfigDisplayViewName` | "ACEScg" 查找非空；默认 display 的 view 数 >0；view 名非空 |
| Processor 变体 | `ProcessorCreateTransformForwardInverse`、`ProcessorCreateDisplay` | direction=0 和 1 均成功；display processor 非空 |
| LUT | `ProcessorCreateFromLutMissing` | 缺失路径返回 null；不崩溃（LUT 实现为 stub，先测失败路径） |
| Display Transform Apply | `DisplayTransformApply`、`DisplayTransformApplyExposure` | apply 后像素值改变；exposure 调整使亮度统计量变化 |
| GPU Shader 元数据 | `GpuShader3dLutCount`、`GpuShaderGet3dLut`、`GpuShaderTextureCount`、`GpuShaderGetTexture` | display transform 产生的 shader 有 ≥0 个 LUT；get 返回 name/sampler/edge_len/values；非法 index 返回 -1 |
| GradingPrimary | `GradingPrimarySetContrast`、`SetOffset`、`SetExposure`、`SetSaturationGray`、`SetClamp` | contrast 2x 后暗部被压缩；offset 红色偏移；saturation=0 后 RGB 通道统计量一致（灰度）；clamp 后像素值在范围内 |
| 颜色空间等价 | `ColorSpaceEqual` | 同一 handle 返回 true；不同返回 false；null 返回 false |

#### 2.3 `c_api_renderer_test.cpp` 扩展（GPU 相关）

oakgl 的所有测试均依赖 OpenGL context。策略：**本地开发环境必须完整运行；CI headless 环境允许条件跳过，但需通过 `QT_QPA_PLATFORM=offscreen` 或 mock context 尽量执行。**

| 新增测试组 | 用例 | 验证点 | CI 策略 |
|---|---|---|---|
| Texture 上传变体 | `TextureUploadFromFrame`、`TextureCreatePlanar` | from_frame 带 stride 上传成功；planar YUV 创建成功 | 尽量运行，无 context 则 skip |
| Target 管理 | `TargetDetachColorTexture` | detach 返回非空纹理；detach 后 target color_texture 为 null | 尽量运行 |
| 绘制命令 | `ClearTexture`、`DrawLines`、`DrawPolygon` | clear 后 readback 像素为指定颜色；lines/polygon 不崩溃 | 尽量运行 |
| Shader v2 | `DrawWithShaderEx`、`DrawWithShaderToTextureEx` | binary uniform 传入 float/vec4/mat4；绘制后 dest target readback 非全零 | 尽量运行 |
| YUV Blit | `BlitYuvToRgba` | 创建 Y/U/V 三个 planar texture；blit 后 dest readback 为 RGBA 非零 | 尽量运行 |

> **CI 兼容性方案**：
> 1. Linux CI 安装 `xvfb` 或 `mesa` software renderer，设置 `QT_QPA_PLATFORM=offscreen` 或 `xcb`。
> 2. macOS CI 使用 `QT_QPA_PLATFORM=offscreen`（macOS 原生 offscreen 支持）。
> 3. Windows CI 使用 `QT_QPA_PLATFORM=windows:offscreen` 或 Angle。
> 4. 若上述方式均失败，允许 `GTEST_SKIP()`，但跳过原因必须明确打印环境信息，便于排查。

#### 2.4 `c_api_engine_test.cpp` 扩展

| 新增测试组 | 用例 | 验证点 |
|---|---|---|
| 边界时间 | `RenderFrameAtDuration`、`RenderFrameLargeTime` | 项目时长边界处渲染不崩溃；超大时间值返回错误或最后一帧 |
| 空项目 | `SessionCreateZeroNodes` | 0 节点项目 session create 成功；render 返回黑色帧或空帧 |
| 像素格式 | `SessionCreateInvalidPixelFormat` | 非法 enum 值返回 null |

**注意**：engine session 和 renderer 在无显示环境下需要 `QT_QPA_PLATFORM=offscreen` 或 mock GL 上下文。若 CI 仍无法创建 OpenGL context，需在 fixture 中优雅跳过并记录，但尽量在本地开发机上完整运行。

#### 2.5 `c_api_integration_test.cpp` 扩展

| 新增场景 | 步骤 | 验证点 |
|---|---|---|
| Encode Roundtrip | engine render 10 帧 → encoder 写入 → finalize → decoder 重新打开读取 | 解码后宽高与原始一致；像素 MSE 低于阈值 |
| Conform → Audio Decode | decoder_open WAV → conform 到 48k/mono → poll 到 100 → read_audio_ex 带 cache_path | 返回样本数正确；与参考重采样数据误差在容差内 |
| GPU Shader → Engine Preview | display transform processor → gpu_shader_create → 验证 shader text 含 function_name → 验证 3D LUT count | shader 非空；LUT 维度为 OCIO cube 尺寸（如 33×33×33） |
| dlopen 并发安全 | 同进程同时加载 engine+codec+color；3 线程并发调用；逆序卸载 | 无崩溃；无符号解析错误；ASan 零泄漏 |

---

### 阶段三：P2 — 边缘与负面用例矩阵

对每个 C API 函数补充以下类别的测试：

| 类别 | 示例 | 期望行为 |
|---|---|---|
| Null handle | 所有 `*_destroy` / `*_free` 传 NULL | 不崩溃 |
| Double-free | `oak_frame_free` 后再次 free | 不崩溃（内部设 null 保护） |
| 零尺寸 | `oak_frame_alloc(0,0,fmt)` | 返回 null |
| 极端尺寸 | `width=1,height=1` 和 `16384×16384` | 小尺寸成功；大尺寸优雅 OOM 或返回 null |
| 非法 enum | pixel_format = -1 或 999 | 返回错误码；不崩溃 |
| 空字符串 | filepath=""、space_name="" | 返回 null 或错误码 |
| 并发访问 | 2 线程同时对同一 decoder 调用 read_video | 串行化或返回错误；不崩溃 |

---

## 测试数据 / Assets

放置于 `tests/assets/c_api/`（Git LFS 或小型二进制文件）：

| Asset | 用途 | 大小目标 | 生成方式 |
|---|---|---|---|
| `test_10frames_1920x1080_h264.mp4` | Video decode、thumbnail、engine render | < 500 KB | `ffmpeg -f lavfi -i testsrc=duration=0.4:size=1920x1080:rate=25 -pix_fmt yuv420p -c:v libx264` |
| `test_1sec_48khz_stereo_float.wav` | Audio decode、conform、encoder audio track | < 200 KB | `ffmpeg -f lavfi -i sine=frequency=1000:duration=1 -ac 2 -ar 48000 -sample_fmt flt` |
| `test_1frame_4k_png.png` | OIIO decoder path、high-res frame alloc | < 100 KB | 程序生成纯色图 |
| `identity_3dlut.cube` | LUT processor test | < 10 KB | 手写 33-point identity LUT |
| `ocio_config_v2/config.ocio` | Minimal OCIO v2 config with ACEScg、sRGB、Rec.709 | < 50 KB | 从 OCIO 官方示例裁剪 |
| `minimal_project.ove.xml` | Single Viewer → Media node graph for engine load/render | < 5 KB | 手写 |

---

## 模块覆盖映射（现有内部模块）

每个顶层模块至少有一个测试用例。

- `app/common`：`common_current_test.cpp`、`common_xmlutils_test.cpp`
- `app/config`：`config_test.cpp`
- `app/node`：`node_value_test.cpp`、`node_keyframe_test.cpp`、`node_serialization_test.cpp`
- `app/node/project/serializer`：`project_serializer_test.cpp`
- `app/render`：`render_videoparams_test.cpp`、`render_audioparams_test.cpp`
- `app/timeline`：`timeline_marker_test.cpp`
- `app/undo`：`undo_stack_test.cpp`
- `app/task`：`task_taskmanager_test.cpp`
- `app/codec`：`codec_frame_test.cpp`
- `app/pluginSupport`：`plugin_support_test.cpp`
- `app/audio`、`app/cli`、`app/dialog`、`app/panel`、`app/tool`、`app/ui`、`app/widget`、`app/window`：`module_smoke_test.cpp`

若模块包含 GUI 依赖，则测试聚焦于其非可视逻辑/数据结构。

---

## 集成测试说明

### 项目序列化回归
- 创建最小项目并添加内置节点。
- 使用 `ProjectSerializer::Save` 写出 XML。
- 再用 `ProjectSerializer::Load` 读回。
- 验证节点恢复。

### 任务管理器执行
- 向 `TaskManager` 添加一个 DummyTask。
- 使用事件循环等待完成。
- 验证任务确实执行。

---

## 单元覆盖重点（已扩展）

- `app/undo`：`undo_stack_test.cpp` 覆盖空栈状态、模型数据、redo 区域颜色、jump 行为、空 MultiUndoCommand 忽略逻辑。
- `app/timeline`：`timeline_marker_test.cpp` 覆盖列表排序、最近 marker 查询、含未知元素的保存/加载、marker 增删改命令。
- `app/pluginSupport`：`plugin_support_image_test.cpp` 覆盖 OFX 属性映射（bounds/ROD、像素深度、通道、预乘）及分配/清理行为。
- `app/render`：`render_videoparams_branch_test.cpp` 覆盖自动 divider、像素宽高比校验、方形像素宽度、Save/Load 回归。

---

## 无 GUI 运行

- 测试避免使用 QWidget。
- CI 中设置 `QT_QPA_PLATFORM=offscreen` 防止 GUI 初始化问题。
- oakengine / oakgl 的 GPU 相关测试若无法创建 context，允许跳过，但需在本地 GPU 环境定期回归。

---

## 持续集成

CI 在 Windows/macOS/Linux 上执行：

1. 安装依赖（Qt、FFmpeg、OpenImageIO、OpenColorIO、OpenEXR、PortAudio、Expat）。
2. `-DBUILD_TESTS=ON` 配置。
3. 使用 CMake + Ninja 构建。
4. 运行 `ctest` 输出失败信息。

### 新增 CI 步骤

```yaml
- name: C API Full-Coverage Tests
  run: |
    cd build
    ctest -R olive-gtest --output-on-failure
    # ASan build（可选，Debug 配置下启用）
    cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address"
    cmake --build . --target olive-gtest
    ./tests/gtest/olive-gtest --gtest_filter="CAP*"
```

### 依赖安装说明
- Linux：优先使用发行版系统包（Ubuntu 上用 `apt`）安装 Qt6、FFmpeg、OpenImageIO、OpenColorIO、OpenEXR、PortAudio、Expat、OpenGL 头文件。
- macOS：使用 Homebrew 安装 Qt6 和图像/色彩/多媒体相关库。
- Windows：尽量使用系统安装器（Qt 通过 `install-qt-action`），其余 C/C++ 库通过 vcpkg 安装。

---

## 新增测试规范

- 新测试放在 `tests/gtest`。
- 使用 GoogleTest 规范。
- 尽量保持确定性与无外部依赖。
- 新模块至少增加 1 个单元测试 + 1 个集成场景（可合并）。
- **不允许引入永久性 `GTEST_SKIP()`**。若某些测试必须在特定硬件/环境下运行，使用运行时检测 + 条件跳过，并在测试输出中打印跳过原因和环境信息。

---

## 完成标准

- [ ] `c_api_audio_test.cpp` 编译通过，全部用例通过（oakaudio 函数级覆盖 ≥90%）。
- [ ] `c_api_pluginhost_test.cpp` 编译通过（oakpluginhost stub 安全覆盖 ≥80%）。
- [ ] `c_api_codec_test.cpp` 中所有被跳过的 format 映射测试恢复为正常断言，新增 decoder 真实文件、video/audio 解码、conform、encoder 完整链路用例（oakcodec 函数级覆盖 ≥90%）。
- [ ] `c_api_color_test.cpp` 新增 display transform apply、GPU shader LUT/texture、grading primary 全部 setter、processor 变体（oakcolor 函数级覆盖 ≥90%）。
- [ ] `c_api_renderer_test.cpp` 新增 texture 变体、target detach、绘制命令、shader v2、YUV blit（oakgl 函数级覆盖 ≥85%）。
- [ ] `c_api_engine_test.cpp` 新增边界时间、空项目、非法像素格式用例；headless CI 中尽可能 mock GPU 或通过 offscreen platform 执行 session 测试。
- [ ] `c_api_integration_test.cpp` 新增 encode roundtrip、conform→audio decode、GPU shader→engine preview、dlopen 并发安全场景。
- [ ] ASan/UBSan 运行零泄漏、零 use-after-free、零未定义行为。
- [ ] `ctest` 在 macOS/Linux/Windows 全绿。
