# oakcolor.so C API 设计

> 色彩管理：OCIO 配置管理、色彩空间转换、LUT 应用、显示变换。  
> 内部使用 OpenColorIO，对外完全隐藏 OCIO C++ API。  
> **核心约定**：Oak 全链路内部工作空间为 **ACEScg (AP1 primaries, scene-referred linear)**。所有模块通过 `oakcolor.so` 做 IDT（输入转换）和 ODT（输出转换）。
>
> **IDT/ODT 调用方**：
> - **IDT**：由 `oakcodec.so` 在解码器输出时调用（源文件色彩空间 → ACEScg）。
> - **ODT**：由 `oakcodec.so` 在编码器写入前调用（ACEScg → 目标编码空间）。
> - **View Transform**：由 `oakengine.so` 在预览窗口显示前调用（ACEScg → 显示空间）。
> - **节点内部转换**：由 `oaknodes.so` 的 `PluginNode` 在调用不支持 ACEScg 的 OFX 插件时调用。

## 一、类型定义

```c
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakColorConfig*    OakColorConfigHandle;
typedef struct OakColorProcessor* OakColorProcessorHandle;
typedef struct OakColorSpace*     OakColorSpaceHandle;
typedef struct OakDisplayTransform* OakDisplayTransformHandle;
```

## 二、配置管理

```c
/**
 * @brief 加载 OCIO 配置文件。
 * @param config_path OCIO 配置文件路径（.ocio），传 NULL 使用内置默认配置。
 * @return 配置句柄，NULL 表示加载失败。
 */
OakColorConfigHandle oak_color_config_load(const char* config_path);

/**
 * @brief 释放配置句柄。
 */
void oak_color_config_free(OakColorConfigHandle config);

/**
 * @brief 获取配置中的色彩空间数量。
 */
int oak_color_config_space_count(OakColorConfigHandle config);

/**
 * @brief 获取指定索引的色彩空间名称。
 * @param index 索引（0-based）。
 * @return 色彩空间名称字符串（常量指针，无需释放）。
 */
const char* oak_color_config_space_name(OakColorConfigHandle config, int index);

/**
 * @brief 根据名称查找色彩空间。
 * @param name 色彩空间名称，如 "ACES - ACEScg"。
 * @return 色彩空间句柄，NULL 表示不存在。
 */
OakColorSpaceHandle oak_color_config_get_space(OakColorConfigHandle config, const char* name);

/**
 * @brief 获取默认输入色彩空间（用于未标记的素材）。
 */
const char* oak_color_config_default_input_space(OakColorConfigHandle config);

/**
 * @brief 获取默认显示设备名称。
 */
const char* oak_color_config_default_display(OakColorConfigHandle config);

/**
 * @brief 获取指定显示设备下的视图数量。
 */
int oak_color_config_display_view_count(OakColorConfigHandle config, const char* display_name);

/**
 * @brief 获取指定显示设备下的视图名称。
 */
const char* oak_color_config_display_view_name(OakColorConfigHandle config, const char* display_name, int view_index);
```

## 三、色彩处理器

```c
/**
 * @brief 创建色彩转换处理器。
 * @param config 配置句柄。
 * @param src_space_name 源色彩空间名称。
 * @param dst_space_name 目标色彩空间名称。
 * @return 处理器句柄，NULL 表示失败（如色彩空间不存在）。
 * @note 处理器创建后可以多次使用，是线程安全的。
 * @note 全链路 ACEScg 中最常用的转换对：
 *       - IDT: "Input - Rec.709" → "ACES - ACEScg"
 *       - IDT: "Input - ARRI - V3 LogC (EI800) - Wide Gamut" → "ACES - ACEScg"
 *       - IDT: "Input - Apple - Apple Log" → "ACES - ACEScg"
 *       - ODT: "ACES - ACEScg" → "Output - Rec.709"
 *       - ODT: "ACES - ACEScg" → "Output - Rec.2020 PQ"
 *       - ODT: "ACES - ACEScg" → "Output - P3-DCI"
 *       - 显示: "ACES - ACEScg" → "Output - sRGB" (通过 View Transform)
 */
OakColorProcessorHandle oak_color_processor_create(OakColorConfigHandle config,
                                                     const char* src_space_name,
                                                     const char* dst_space_name);

/**
 * @brief 基于 LUT 文件创建处理器。
 * @param lut_path LUT 文件路径（.cube、.spi3d 等）。
 * @param input_space LUT 输入侧的色彩空间。
 * @param output_space LUT 输出侧的色彩空间。
 */
OakColorProcessorHandle oak_color_processor_create_from_lut(const char* lut_path,
                                                            const char* input_space,
                                                            const char* output_space);

void oak_color_processor_free(OakColorProcessorHandle processor);

/**
 * @brief 对图像缓冲区进行色彩转换（CPU 路径）。
 * @param processor 处理器句柄。
 * @param width 图像宽度。
 * @param height 图像高度。
 * @param in_data 输入数据（RGBA float32，非交错或交错取决于 pix_layout）。
 *                数据应在 scene-referred linear 空间。
 * @param out_data 输出数据（调用者分配，大小与输入相同）。
 * @param pix_layout 像素布局：0 = 交错 RGBA，1 = planar（RRRR...GGGG...BBBB...AAAA...）。
 * @return 0 成功，非 0 失败。
 * @note 这是 IDT/ODT 的 CPU 实现。GPU 路径应通过 oak_renderer_draw_with_shader + OCIO GPU LUT 实现。
 */
int oak_color_processor_apply(OakColorProcessorHandle processor,
                              int width, int height,
                              const float* in_data, float* out_data,
                              int pix_layout);

/**
 * @brief 对单像素进行色彩转换（快速查询）。
 * @param in_rgba 输入 RGBA float32[4]。
 * @param out_rgba 输出 RGBA float32[4]（可与输入同地址）。
 */
void oak_color_processor_apply_pixel(OakColorProcessorHandle processor,
                                     const float* in_rgba, float* out_rgba);
```

## 四、显示变换（用于预览/ viewer）

```c
/**
 * @brief 创建显示变换（用于 viewer 预览）。
 * @param config 配置句柄。
 * @param input_space 输入图像的色彩空间。全链路 ACEScg 下固定为 "ACES - ACEScg"。
 * @param display_name 显示设备名称（如 "sRGB"、"Rec.709"、"Rec.2020"、"P3"）。
 * @param view_name 视图名称（如 "ACES 1.0 SDR-video"、"ACES 1.0 HDR-video"）。
 * @param look_name 可选的 Look 名称（NULL 表示无）。如 "Neutral"、"Warm"、"Cool"。
 * @param exposure_fstop 曝光调整（EV）。用于预览时的曝光微调，不影响实际输出。
 * @param display_gamma 显示 Gamma 调整。
 * @return 显示变换处理器句柄。
 * @note View Transform = RRT (Reference Render Transform) + ODT (Output Device Transform)。
 *       它把 scene-referred linear 的 ACEScg 数据转换成 display-referred 的非线性数据，适合直接显示在屏幕上。
 *       **严禁跳过 View Transform 直接显示 ACEScg 数据**，否则画面会发灰、过曝。
 */
OakDisplayTransformHandle oak_display_transform_create(OakColorConfigHandle config,
                                                        const char* input_space,
                                                        const char* display_name,
                                                        const char* view_name,
                                                        const char* look_name,
                                                        float exposure_fstop,
                                                        float display_gamma);

void oak_display_transform_free(OakDisplayTransformHandle transform);

/**
 * @brief 应用显示变换到图像。
 * @param transform 显示变换句柄。
 * @param width 图像宽度。
 * @param height 图像高度。
 * @param in_data 输入 RGBA float32。
 * @param out_data 输出 RGBA float32（调用者分配）。
 * @param pix_layout 像素布局（0 = 交错，1 = planar）。
 */
int oak_display_transform_apply(OakDisplayTransformHandle transform,
                                int width, int height,
                                const float* in_data, float* out_data,
                                int pix_layout);
```

## 五、元数据与参考空间

```c
/**
 * @brief 获取参考色彩空间名称（如 "ACES - ACES2065-1"）。
 */
const char* oak_color_config_reference_space_name(OakColorConfigHandle config);

/**
 * @brief 判断两个色彩空间是否等价（无需转换）。
 */
bool oak_color_space_equal(OakColorSpaceHandle a, OakColorSpaceHandle b);

#ifdef __cplusplus
}
#endif
```
