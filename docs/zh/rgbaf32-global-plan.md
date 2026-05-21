# 素材读入强制转换为 RGBAF32 并内部全链路使用 F32 处理 — 实施计划

## 1. 现状分析

### 1.1 视频读入位置

视频/图像素材在以下位置被读入并解码为 GPU Texture：

| 层级 | 文件 | 职责 |
|------|------|------|
| 解码接口 | `app/codec/decoder.h` / `.cpp` | 基类 `Decoder`，定义 `RetrieveVideo(RetrieveVideoParams)` 公共接口 |
| FFmpeg 解码 | `app/codec/ffmpeg/ffmpegdecoder.cpp` | `FFmpegDecoder::RetrieveVideoInternal()` —— 核心视频解码路径 |
| OIIO 解码 | `app/codec/oiio/oiiodecoder.cpp` | `OIIODecoder::RetrieveVideoInternal()` —— 静态图片解码路径 |
| 渲染触发 | `app/render/renderprocessor.cpp` | `ProcessVideoFootage()` —— 在节点图遍历中触发解码，并做颜色管理转换 |
| 遍历调度 | `app/node/traverser.cpp` | `ResolveJobs()` —— 将 `FootageJob` 分发给 `ProcessVideoFootage()` |

**数据流：**
```
文件 → FFmpegDecoder::RetrieveVideoInternal()
  → RetrieveFrame() 解码出 AVFrame
  → PreProcessFrame() CPU 缩放/格式转换 (sws_scale_frame)
  → ProcessFrameIntoTexture() 上传为 GPU Texture
    → YUV 格式：上传为 3 个 plane texture + YUV→RGB shader
    → RGBA/RGBA64LE：直接 glTexSubImage2D 上传
  → RenderProcessor::ProcessVideoFootage()
    → BlitColorManaged() OCIO 颜色空间转换 shader
    → 进入节点图后续处理
```

### 1.2 像素格式体系

- **核心枚举：** `ext/core/include/olive/core/render/pixelformat.h` 定义 `PixelFormat::U8 / U16 / F16 / F32`
- **GPU 格式映射：** `app/render/opengl/openglrenderer.cpp` 已将 `F32 + 4ch` 映射到 `GL_RGBA32F / GL_FLOAT`
- **内部工作格式：** `NodeTraverser::GetCacheVideoParams().format()` 决定节点图内部缓存格式
- **项目默认配置：** `app/config/config.cpp` 中 `OnlinePixelFormat = F32`，`OfflinePixelFormat = F16`，说明设计意图就是在线编辑使用 F32

### 1.3 当前 F32 支持的关键缺失

1. **`FFmpegDecoder::GetNativePixelFormat()` 不识别 F32 FFmpeg 格式**
   - 仅映射 `RGBA → U8`、`RGBA64 → U16`
   - `AV_PIX_FMT_RGBAF32`、`AV_PIX_FMT_RGBF32` 等落入 `default: INVALID`

2. **`IsPixelFormatGLSLCompatible()` 未将 RGBAF32 列为 GLSL 兼容**
   - 这会导致即使解码器输出 RGBAF32，也会强制走 `sws_scale_frame` CPU 转换路径

3. **`ProcessFrameIntoTexture()` 直接上传路径缺少 RGBAF32 分支**
   - 当前只有 `YUV...` 和 `RGBA / RGBA64LE` 两个直接上传分支，没有 `RGBAF32` 等直接上传路径

4. **`PreProcessFrame()` 的 `sws_scale_frame` 目标格式选择需验证 F32 支持**
   - `FFmpegUtils::GetCompatiblePixelFormat(..., maximum=F32)` 理论上应返回 `AV_PIX_FMT_RGBAF32`，但需实测验证

5. **OIIO 解码器已原生支持 F32（FLOAT → F32），无需修改**

## 2. 目标

- **读入时转换：** 无论源素材格式（YUV、U8、U16、F16 等），在解码器层面统一转换为 **RGBAF32** 后上传 GPU
- **内部全链路 F32：** 节点图遍历、效果处理、合成、缓存等内部环节全部使用 `PixelFormat::F32`（4 通道）
- **导出保持灵活：** 导出/编码时从 F32 转换为目标格式，保持现有编码逻辑

## 3. 实施方案：全局强制 F32

**思路：** 将 F32 作为唯一的内部工作格式，在解码器出口强制转换。

**改动点：**

1. **解码器层强制 F32 输出**
   - `FFmpegDecoder::RetrieveVideoInternal()`：
     - 修改 `RetrieveVideoParams` 或内部逻辑，令 `maximum_format` 固定为 `F32`
     - 在 `PreProcessFrame()` 中，若源格式非 RGBAF32，通过 `sws_scale_frame` 转换到 `AV_PIX_FMT_RGBAF32`
     - 在 `ProcessFrameIntoTexture()` 中增加 `AV_PIX_FMT_RGBAF32` 直接上传分支（`GL_RGBA32F / GL_FLOAT`）
   - `OIIODecoder::RetrieveVideoInternal()`：
     - OIIO 读入后，若格式非 F32，通过 `Frame::convert(PixelFormat::F32)` 转换，再上传

2. **修复 F32 格式映射**
   - `FFmpegDecoder::GetNativePixelFormat()` 增加 `AV_PIX_FMT_RGBAF32 → PixelFormat::F32`、`AV_PIX_FMT_RGBF32 → PixelFormat::F32`
   - `FFmpegDecoder::GetNativeChannelCount()` 增加对应分支
   - `IsPixelFormatGLSLCompatible()` 增加 `AV_PIX_FMT_RGBAF32`（可选，因为强制转换后解码器输出就是 RGBAF32）

3. **内部工作格式锁定 F32**
   - 在 `NodeTraverser` 初始化或 `RenderProcessor` 创建时，`SetCacheVideoParams()` 强制 `format = PixelFormat::F32`
   - 移除用户层对工作格式的可选配置（或保留配置但忽略/默认 F32）
   - `traverser.cpp` 中 `FootageJob`、`GenerateJob`、`ColorTransformJob` 的格式设置已经使用 `GetCacheVideoParams().format()`，因此只需确保基类参数是 F32 即可

4. **导出层适配**
   - `FFmpegEncoder` 的输入当前通过 `avfilter` 图做格式转换，源为 F32 时：
     - `FFmpegUtils::GetFFmpegPixelFormat(F32, 4)` 已返回 `AV_PIX_FMT_RGBAF32`
     - 验证 filter graph 的 `buffer` source 和 `format` filter 能否正确处理 `RGBAF32`
   - `RenderProcessor::GenerateFrame()` 下载 GPU texture 到 `FramePtr` 时，`DownloadFromTexture()` 已支持 `GL_FLOAT`，直接得到 F32 CPU buffer

## 4. 关键文件与修改清单

| 文件 | 修改内容 |
|------|----------|
| `app/codec/ffmpeg/ffmpegdecoder.cpp` | ① `GetNativePixelFormat()` 增加 RGBAF32/RGBF32 → F32 映射<br>② `GetNativeChannelCount()` 增加对应分支<br>③ `IsPixelFormatGLSLCompatible()` 增加 RGBAF32<br>④ `ProcessFrameIntoTexture()` 增加 RGBAF32 直接上传分支<br>⑤ `PreProcessFrame()` 确保 divider=1 且格式为 RGBAF32 时跳过 CPU 转换 |
| `app/codec/oiio/oiiodecoder.cpp` | `RetrieveVideoInternal()` 上传前若 `frame.format() != F32` 则调用 `convert(F32)` |
| `app/node/traverser.cpp` 或 `app/render/renderprocessor.cpp` | 初始化时强制 `SetCacheVideoParams().format = F32` |
| `app/codec/ffmpeg/ffmpegencoder.cpp` | 验证 filter graph 对 RGBAF32 source 的处理，必要时调整 |
| `app/render/opengl/openglrenderer.cpp` | 确认 `GL_RGBA32F / GL_FLOAT` 路径完整，补充必要错误检查 |
| `app/codec/ffmpeg/ffmpegutils.cpp` | 验证 `GetCompatiblePixelFormat(maximum=F32)` 的行为 |

## 5. 风险评估

| 风险 | 说明 | 缓解措施 |
|------|------|----------|
| 内存带宽 ×4 | F32 是 U8 的 4 倍、U16/F16 的 2 倍，显存和内存占用显著增加 | 这是预期代价；`OfflinePixelFormat` 机制可继续用于代理预览，降低分辨率同时用 F16 减少带宽 |
| FFmpeg swscale 对 RGBAF32 支持 | `sws_scale_frame` 是否能正确处理 `AV_PIX_FMT_RGBAF32` 作为目标格式需验证 | 先写单元测试验证；若不支持，可用 OIIO `Frame::convert()` 作为 fallback，或在 GPU 上通过 shader 做格式转换 |
| OFX 插件兼容性 | 大部分 OFX 插件支持 `kOfxBitDepthFloat`，但仍有少数可能只支持 U8/U16 | `PluginRenderer` 已有格式转换路径，F32 的支持比 F16 更成熟 |
| 性能回归 | YUV→RGB 原来在 GPU 走 shader，若强制先转 RGBAF32 再上传，可能需要调整流程 | YUV 素材仍保留 GPU shader 转换路径，只是 shader 输出目标 texture 格式改为 F32（OpenGL 已支持 `GL_RGBA32F` 作为 render target） |
| 缓存文件体积翻倍 | 帧缓存从 U8/U16 改为 F32 后，磁盘缓存体积增大 | 可接受；必要时调整缓存策略或压缩 |

## 6. 建议的实施顺序

1. **第一阶段：** 修复 `FFmpegDecoder` F32 映射 + 增加 RGBAF32 直接上传分支，编写解码器单元测试
2. **第二阶段：** 在 `OIIODecoder` 添加强制 F32 转换
3. **第三阶段：** 锁定内部工作格式为 F32，验证节点图全链路
4. **第四阶段：** 验证导出编码路径，确认 filter graph 对 RGBAF32 的处理
5. **第五阶段：** 性能测试与回归测试

## 7. 决策点

- 是否保留 `OfflinePixelFormat = F16` 的代理降级机制？还是连 proxy 也强制 F32？
- 若保留代理降级，是否需要在解码器层根据 online/offline 模式选择输出格式？
