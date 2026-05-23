# oakpluginhost.so C API 设计

> OpenFX 插件宿主。对外暴露两部分接口：
> 1. 符合 OpenFX 官方 C ABI 标准的 Host Suite（供插件调用）。
> 2. oak 特有的宿主注册/查询接口（供主进程控制插件加载）。
>
> **核心约定**：Oak 全链路内部工作空间为 **RGBA32F + ACEScg**。  
> OFX 插件的 `kOfxBitDepthFloat` 默认被解释为 ACEScg linear。若插件不支持 ACEScg，
> `PluginNode`（在 `oaknodes.so` 中）负责在调用插件前/后做色彩空间桥接。

## 一、OpenFX 标准 Host Suite

这部分已存在于 `third_party/openfx/HostSupport` 中，是 OpenFX 官方定义的 C API：

```c
// OpenFX 标准接口（OfxCore.h / ofxImageEffect.h）
// 插件通过 OfxHost 结构体指针获取宿主能力

typedef struct OfxHost {
    OfxPropertySetHandle host;
    OfxStatus (*fetchSuite)(OfxPropertySetHandle host, const char* suiteName, int suiteVersion, const void** suite);
} OfxHost;
```

`oakpluginhost.so` 负责实现 `fetchSuite` 中返回的各个 Suite：
- `OfxPropertySuiteV1`
- `OfxParameterSuiteV1`
- `OfxImageEffectSuiteV2`
- `OfxMemorySuiteV1`
- `OfxMessageSuiteV1`
- `OfxMultiThreadSuiteV1`
- `OfxProgressSuiteV1`
- `OfxTimeLineSuiteV1`
- `OfxInteractSuiteV1`

> 这些接口的签名和语义完全遵循 OpenFX 1.4 规范，此处不再重复。见 `third_party/openfx/include/ofx*.h`。

## 二、oak 特有的宿主控制接口

```c
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakPluginHost* OakPluginHostHandle;
typedef struct OakPlugin*     OakPluginHandle;
typedef struct OakPluginInstance* OakPluginInstanceHandle;

/**
 * @brief 插件描述信息（POD）。
 */
typedef struct {
    const char* id;           // 插件唯一 ID，如 "com.example.MyBlur"
    const char* name;         // 显示名称
    const char* version;      // 版本号
    const char* description;  // 描述
    const char* group;        // 分类组名，如 "Color"、"Filter"
    int         major_version;
    int         minor_version;
} OakPluginInfo;
```

### 2.1 宿主生命周期

```c
/**
 * @brief 创建插件宿主上下文。
 * @param host_name 宿主名称字符串，如 "OakVideoEditor"（用于插件识别宿主）。
 * @return 宿主句柄，NULL 表示失败。
 */
OakPluginHostHandle oak_plugin_host_create(const char* host_name);

/**
 * @brief 销毁宿主上下文，卸载所有已加载插件。
 */
void oak_plugin_host_destroy(OakPluginHostHandle host);

/**
 * @brief 设置宿主能力（供插件查询）。
 * @param capability 能力名称："supports_overlays"、"supports_multires"、"supports_opencl" 等。
 * @param value 能力值（整数）。
 */
void oak_plugin_host_set_capability(OakPluginHostHandle host,
                                    const char* capability, int value);
```

### 2.2 插件加载与枚举

```c
/**
 * @brief 从文件系统加载一个 OFX 插件包（.ofx.bundle 目录或单个 .ofx 文件）。
 * @param host 宿主句柄。
 * @param bundle_path 插件包路径。
 * @return 0 成功，非 0 失败。
 */
int oak_plugin_load_bundle(OakPluginHostHandle host, const char* bundle_path);

/**
 * @brief 扫描目录并加载所有 OFX 插件包。
 * @param host 宿主句柄。
 * @param search_path 搜索路径（可包含多个路径，用系统路径分隔符分隔）。
 * @return 加载成功的插件数量。
 */
int oak_plugin_load_from_path(OakPluginHostHandle host, const char* search_path);

/**
 * @brief 获取已加载插件数量。
 */
int oak_plugin_count(OakPluginHostHandle host);

/**
 * @brief 获取指定索引的插件信息。
 * @param index 插件索引（0-based）。
 * @param out_info 输出信息结构体。
 * @return 0 成功，非 0 失败（索引越界）。
 */
int oak_plugin_get_info(OakPluginHostHandle host, int index, OakPluginInfo* out_info);

/**
 * @brief 根据 ID 查找已加载插件。
 * @param plugin_id 插件 ID。
 * @return 插件句柄，NULL 表示未找到。
 */
OakPluginHandle oak_plugin_find(OakPluginHostHandle host, const char* plugin_id);
```

### 2.3 插件实例生命周期

```c
/**
 * @brief 创建插件实例（对应 OFX ImageEffect instance）。
 * @param plugin 插件句柄。
 * @param context 上下文名称："Filter"、"General"、"Transition"、"Generator"。
 * @param project_width 项目宽度。
 * @param project_height 项目高度。
 * @param pixel_aspect 像素宽高比。
 * @param frame_rate 帧率（有理数，num/den）。
 * @return 实例句柄，NULL 表示失败。
 */
OakPluginInstanceHandle oak_plugin_instance_create(OakPluginHandle plugin,
                                                   const char* context,
                                                   int project_width, int project_height,
                                                   double pixel_aspect,
                                                   int64_t frame_rate_num, int64_t frame_rate_den);

void oak_plugin_instance_destroy(OakPluginInstanceHandle instance);

/**
 * @brief 获取实例当前的时间线时间。
 * @return 时间（秒，浮点数）。
 */
double oak_plugin_instance_time(OakPluginInstanceHandle instance);

/**
 * @brief 设置实例的时间线时间（触发插件的 instanceChangedAction）。
 */
void oak_plugin_instance_set_time(OakPluginInstanceHandle instance, double t);

/**
 * @brief 渲染一帧。
 * @param instance 实例句柄。
 * @param time 渲染时间点（秒）。
 * @param render_scale_x 渲染缩放 X（用于代理/预览）。
 * @param render_scale_y 渲染缩放 Y。
 * @param input_textures 输入纹理数组（对应插件的 clip）。
 *                       每个元素为 OakTextureHandle。纹理格式应为 RGBA32F（ACEScg）。
 *                       若插件声明的色彩空间不是 ACEScg，由 PluginNode 在调用前转换。
 * @param input_count 输入数量。
 * @param output_target 输出目标（OakTargetHandle 或 OakTextureHandle）。
 *                      输出格式应为 RGBA32F（ACEScg）。
 * @return 0 成功，非 0 失败。
 *
 * @note 插件内部的像素格式通过 `kOfxImageEffectPropSupportedPixelDepths` 声明：
 *       - `kOfxBitDepthFloat`（32F）：推荐。Oak 直接传递 ACEScg 数据，零拷贝。
 *       - `kOfxBitDepthHalf`（16F）：Oak 内部做 32F→16F 转换（精度损失可接受）。
 *       - `kOfxBitDepthShort`（16U）/ `kOfxBitDepthByte`（8U）：
 *         Oak 做 32F→8/16U + ACEScg→sRGB 转换。PluginNode 会自动桥接。
 */
int oak_plugin_instance_render(OakPluginInstanceHandle instance,
                               double time,
                               double render_scale_x, double render_scale_y,
                               void** input_textures, int input_count,
                               void* output_target);
```

### 2.4 参数交互

```c
/**
 * @brief 获取实例的参数数量。
 */
int oak_plugin_instance_param_count(OakPluginInstanceHandle instance);

/**
 * @brief 获取参数信息。
 * @param param_index 参数索引。
 * @param out_name 输出参数名称（常量指针）。
 * @param out_type 输出参数类型："Double"、"Integer"、"String"、"RGB"、"RGBA"、"Boolean"、"Choice"。
 */
int oak_plugin_instance_param_info(OakPluginInstanceHandle instance, int param_index,
                                   const char** out_name, const char** out_type);

/* ---- 参数读写 ---- */
void    oak_plugin_instance_set_param_double(OakPluginInstanceHandle instance, const char* name, double val);
double  oak_plugin_instance_get_param_double(OakPluginInstanceHandle instance, const char* name);

void    oak_plugin_instance_set_param_int(OakPluginInstanceHandle instance, const char* name, int val);
int     oak_plugin_instance_get_param_int(OakPluginInstanceHandle instance, const char* name);

void    oak_plugin_instance_set_param_string(OakPluginInstanceHandle instance, const char* name, const char* val);
const char* oak_plugin_instance_get_param_string(OakPluginInstanceHandle instance, const char* name);

void    oak_plugin_instance_set_param_bool(OakPluginInstanceHandle instance, const char* name, bool val);
bool    oak_plugin_instance_get_param_bool(OakPluginInstanceHandle instance, const char* name);

void    oak_plugin_instance_set_param_rgb(OakPluginInstanceHandle instance, const char* name, const float* rgb);
void    oak_plugin_instance_get_param_rgb(OakPluginInstanceHandle instance, const char* name, float* out_rgb);

void    oak_plugin_instance_set_param_rgba(OakPluginInstanceHandle instance, const char* name, const float* rgba);
void    oak_plugin_instance_get_param_rgba(OakPluginInstanceHandle instance, const char* name, float* out_rgba);
```

### 2.5 回调注册（供 oakengine.so / oaknodes.so 使用）

插件实例需要与宿主交互时间线、进度条、对话框等。这些通过回调实现：

```c
typedef double (*OakPluginGetTimeFn)(void* user_data);
typedef void   (*OakPluginGotoTimeFn)(double t, void* user_data);
typedef void   (*OakPluginGetBoundsFn)(double* out_t1, double* out_t2, void* user_data);

typedef struct {
    OakPluginGetTimeFn  get_time;
    OakPluginGotoTimeFn goto_time;
    OakPluginGetBoundsFn get_bounds;
} OakPluginTimelineCallbacks;

/**
 * @brief 设置时间线回调（供插件查询当前时间）。
 */
void oak_plugin_instance_set_timeline_callbacks(OakPluginInstanceHandle instance,
                                                const OakPluginTimelineCallbacks* cbs,
                                                void* user_data);
```

#ifdef __cplusplus
}
#endif
