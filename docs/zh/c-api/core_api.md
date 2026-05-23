# oakcore.so C API 设计

> 节点图基础设施：Node/NodeGraph/NodeValue 类型系统、序列化、连接关系。  
> 全链路默认：节点图内部传递的视频帧为 **RGBA32F + ACEScg**。

## 一、类型定义

```c
#include <stdint.h>
#include <stdbool.h>
#include "oak/frame_api.h"   /* OakFrame */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OakNodeGraph* OakGraphHandle;
typedef struct OakNode*      OakNodeHandle;
typedef struct OakNodeInput* OakNodeInputHandle;
typedef struct OakNodeOutput* OakNodeOutputHandle;
typedef struct OakValue*     OakValueHandle;
typedef struct OakNodeTypeRegistry* OakRegistryHandle;
```

## 二、节点类型注册（供 oaknodes.so 使用）

`oakcore.so` 维护一张全局的节点类型注册表。`oaknodes.so` 在 `dlopen` 后调用注册函数注入自己的工厂。

```c
/**
 * @brief 节点工厂函数签名。
 * @return 新创建的节点实例。oakcore.so 接管所有权，通过 oak_node_destroy 销毁。
 */
typedef OakNodeHandle (*OakNodeFactoryFn)(void);

/**
 * @brief 注册一个节点类型。
 * @param type_id 全局唯一类型标识符，如 "org.oak.Blur"、"org.oak.Footage"。
 * @param factory 创建该类型实例的工厂函数指针。
 * @return 0 成功，非 0 失败（如 type_id 已存在）。
 */
int oak_node_register_type(const char* type_id, OakNodeFactoryFn factory);

/**
 * @brief 注销一个节点类型。通常只在模块卸载时调用。
 * @param type_id 类型标识符。
 */
void oak_node_unregister_type(const char* type_id);

/**
 * @brief 查询已注册的类型列表。
 * @param out_ids 输出字符串数组，由调用者分配。
 * @param max_count out_ids 的最大容量。
 * @return 实际写入的数量。
 */
int oak_node_list_types(const char** out_ids, int max_count);
```

## 三、节点图生命周期

```c
/**
 * @brief 创建空的节点图。
 * @return 节点图句柄，NULL 表示内存不足。
 */
OakGraphHandle oak_graph_create(void);

/**
 * @brief 销毁节点图，同时销毁图中所有节点及其连接。
 * @param graph 节点图句柄。传 NULL 是安全的（无操作）。
 */
void oak_graph_destroy(OakGraphHandle graph);

/**
 * @brief 深拷贝节点图。
 * @param graph 源节点图。
 * @return 新的独立节点图句柄。
 */
OakGraphHandle oak_graph_clone(OakGraphHandle graph);
```

## 四、节点生命周期

```c
/**
 * @brief 通过已注册的类型 ID 创建节点。
 * @param type_id 类型标识符，如 "org.oak.Blur"。
 * @return 节点句柄，NULL 表示类型未注册或内存不足。
 * @note 节点创建后尚未属于任何节点图，需通过 oak_graph_add_node 加入。
 */
OakNodeHandle oak_node_create(const char* type_id);

/**
 * @brief 销毁节点。
 * @param node 节点句柄。传 NULL 是安全的。
 * @note 如果节点仍属于某个节点图，行为未定义。必须先 oak_graph_remove_node。
 */
void oak_node_destroy(OakNodeHandle node);

/**
 * @brief 将节点加入节点图。
 * @param graph 节点图。
 * @param node 节点。加入后所有权转交给图，销毁图时自动销毁节点。
 */
void oak_graph_add_node(OakGraphHandle graph, OakNodeHandle node);

/**
 * @brief 将节点从节点图中移除。
 * @param graph 节点图。
 * @param node 节点。移除时会自动断开所有输入/输出连接。
 */
void oak_graph_remove_node(OakGraphHandle graph, OakNodeHandle node);

/**
 * @brief 获取节点所属图。
 * @param node 节点句柄。
 * @return 所属图句柄，若节点尚未加入任何图则返回 NULL。
 */
OakGraphHandle oak_node_graph(OakNodeHandle node);

/**
 * @brief 获取节点的类型 ID。
 * @param node 节点句柄。
 * @return 类型标识符字符串（常量指针，无需释放）。
 */
const char* oak_node_type_id(OakNodeHandle node);

/**
 * @brief 获取/设置节点标签（显示名称）。
 */
const char* oak_node_label(OakNodeHandle node);
void oak_node_set_label(OakNodeHandle node, const char* label);
```

## 五、参数读写

每个节点暴露一组参数（parameter）。参数通过字符串 ID 标识，类型在运行时确定。

```c
typedef enum {
    OAK_VALUE_VOID = 0,
    OAK_VALUE_BOOL,
    OAK_VALUE_INT,
    OAK_VALUE_INT64,
    OAK_VALUE_FLOAT,
    OAK_VALUE_DOUBLE,
    OAK_VALUE_RATIONAL,      // 分子/分母对
    OAK_VALUE_STRING,
    OAK_VALUE_COLOR,         // RGBA float[4]
    OAK_VALUE_VEC2,          // float[2]
    OAK_VALUE_VEC3,          // float[3]
    OAK_VALUE_VEC4,          // float[4]
    OAK_VALUE_MATRIX,        // float[16] 列主序
    OAK_VALUE_FRAME,         // 视频帧（OakFrame，引用计数）
    OAK_VALUE_AUDIO_BUFFER,  // 音频缓冲区（引用计数）
    OAK_VALUE_CUSTOM = 0xFF  // 节点自定义类型，通过回调访问
} OakValueType;

/**
 * @brief 获取节点参数列表。
 * @param node 节点句柄。
 * @param out_params 输出参数 ID 数组，由调用者分配。
 * @param max_count 最大容量。
 * @return 实际参数数量。
 */
int oak_node_param_list(OakNodeHandle node, const char** out_params, int max_count);

/**
 * @brief 查询参数的数据类型。
 * @param node 节点句柄。
 * @param param_id 参数 ID。
 * @return OakValueType 枚举值。若参数不存在返回 OAK_VALUE_VOID。
 */
OakValueType oak_node_param_type(OakNodeHandle node, const char* param_id);

/* ---- 标量参数 ---- */
void    oak_node_set_bool  (OakNodeHandle node, const char* param_id, bool val);
bool    oak_node_get_bool  (OakNodeHandle node, const char* param_id);

void    oak_node_set_int   (OakNodeHandle node, const char* param_id, int val);
int     oak_node_get_int   (OakNodeHandle node, const char* param_id);

void    oak_node_set_int64 (OakNodeHandle node, const char* param_id, int64_t val);
int64_t oak_node_get_int64 (OakNodeHandle node, const char* param_id);

void    oak_node_set_float (OakNodeHandle node, const char* param_id, float val);
float   oak_node_get_float (OakNodeHandle node, const char* param_id);

void    oak_node_set_double(OakNodeHandle node, const char* param_id, double val);
double  oak_node_get_double(OakNodeHandle node, const char* param_id);

/**
 * @brief 有理数（时间码、帧率等）。
 * @param num 分子。
 * @param den 分母，必须大于 0。
 */
void    oak_node_set_rational(OakNodeHandle node, const char* param_id, int64_t num, int64_t den);
void    oak_node_get_rational(OakNodeHandle node, const char* param_id, int64_t* out_num, int64_t* out_den);

/* ---- 字符串 ---- */
void        oak_node_set_string(OakNodeHandle node, const char* param_id, const char* val);
const char* oak_node_get_string(OakNodeHandle node, const char* param_id); // 常量指针，无需释放

/* ---- 向量/颜色/矩阵（传指针） ---- */
void oak_node_set_vec2 (OakNodeHandle node, const char* param_id, const float* v);
void oak_node_get_vec2 (OakNodeHandle node, const char* param_id, float* out_v);

void oak_node_set_vec3 (OakNodeHandle node, const char* param_id, const float* v);
void oak_node_get_vec3 (OakNodeHandle node, const char* param_id, float* out_v);

void oak_node_set_vec4 (OakNodeHandle node, const char* param_id, const float* v);
void oak_node_get_vec4 (OakNodeHandle node, const char* param_id, float* out_v);

void oak_node_set_color(OakNodeHandle node, const char* param_id, const float* rgba);
void oak_node_get_color(OakNodeHandle node, const char* param_id, float* out_rgba);

void oak_node_set_matrix(OakNodeHandle node, const char* param_id, const float* m16);
void oak_node_get_matrix(OakNodeHandle node, const char* param_id, float* out_m16);
```

## 六、输入/输出连接

```c
/**
 * @brief 获取节点的输入端口列表。
 * @param out_ids 输出端口 ID 数组。
 * @param max_count 最大容量。
 * @return 实际端口数量。
 */
int oak_node_input_list(OakNodeHandle node, const char** out_ids, int max_count);

/**
 * @brief 获取节点的输出端口列表。
 */
int oak_node_output_list(OakNodeHandle node, const char** out_ids, int max_count);

/**
 * @brief 查询输入端口当前连接到的源节点。
 * @param node 目标节点。
 * @param input_id 输入端口 ID。
 * @param out_source_node 输出源节点句柄（可为 NULL）。
 * @param out_source_output_id 输出源端口 ID（缓冲区，由调用者提供）。
 * @param source_output_id_capacity out_source_output_id 缓冲区大小（字节）。
 * @return 0 表示已连接，1 表示未连接，-1 表示 input_id 不存在。
 */
int oak_node_input_connection(OakNodeHandle node, const char* input_id,
                              OakNodeHandle* out_source_node,
                              char* out_source_output_id, int source_output_id_capacity);

/**
 * @brief 建立节点间的连接。
 * @param out_node 源节点。
 * @param output_id 源输出端口 ID。
 * @param in_node 目标节点。
 * @param input_id 目标输入端口 ID。
 * @return 0 成功，非 0 失败（如类型不匹配、形成环）。
 */
int oak_node_connect(OakNodeHandle out_node, const char* output_id,
                     OakNodeHandle in_node, const char* input_id);

/**
 * @brief 断开输入端口的连接。
 * @param node 目标节点。
 * @param input_id 输入端口 ID。
 */
void oak_node_disconnect(OakNodeHandle node, const char* input_id);
```

## 七、求值（类型擦除的值系统）

```c
/**
 * @brief 在指定时间点求值节点的某个输出端口。
 * @param node 节点句柄。
 * @param output_id 输出端口 ID。
 * @param time_num 时间分子。
 * @param time_den 时间分母（大于 0）。
 * @return OakValueHandle，NULL 表示求值失败。调用者必须通过 oak_value_destroy 释放。
 * @note 该函数可能触发递归求值（上游节点）。线程安全取决于具体节点实现。
 */
OakValueHandle oak_node_value_at_time(OakNodeHandle node, const char* output_id,
                                      int64_t time_num, int64_t time_den);

/**
 * @brief 释放值句柄。
 * @param value 值句柄。传 NULL 是安全的。
 */
void oak_value_destroy(OakValueHandle value);

/**
 * @brief 查询值的数据类型。
 */
OakValueType oak_value_type(OakValueHandle value);

/* ---- 标量提取 ---- */
bool    oak_value_get_bool   (OakValueHandle value);
int     oak_value_get_int    (OakValueHandle value);
int64_t oak_value_get_int64  (OakValueHandle value);
float   oak_value_get_float  (OakValueHandle value);
double  oak_value_get_double (OakValueHandle value);
void    oak_value_get_rational(OakValueHandle value, int64_t* out_num, int64_t* out_den);

const char* oak_value_get_string(OakValueHandle value); // 常量指针，无需释放

void oak_value_get_vec2 (OakValueHandle value, float* out_v);
void oak_value_get_vec3 (OakValueHandle value, float* out_v);
void oak_value_get_vec4 (OakValueHandle value, float* out_v);
void oak_value_get_color(OakValueHandle value, float* out_rgba);
void oak_value_get_matrix(OakValueHandle value, float* out_m16);

/* ---- 媒体帧提取（引用计数，零拷贝） ---- */

/**
 * @brief 提取视频帧数据。
 * @param value 值句柄，类型必须是 OAK_VALUE_FRAME。
 * @param out_frame 输出帧描述符（浅拷贝，不转移所有权）。
 *                  调用者**不得**修改 out_frame 的内部数据，**不得**调用 oak_frame_release。
 *                  该帧的生命周期由 value 句柄管理，必须在 oak_value_destroy 前使用。
 * @return 0 成功，非 0 失败（类型不匹配或帧未就绪）。
 *
 * @note 提取出的帧默认格式为 RGBA32F + ACEScg（见 frame_api.h 全链路约定）。
 *       若节点输出非 ACEScg 帧（如 PluginNode），out_frame->colorspace 会标记真实空间。
 */
int oak_value_get_frame(OakValueHandle value, OakFrame* out_frame);

/**
 * @brief 提取音频缓冲区。
 * @param value 值句柄，类型必须是 OAK_VALUE_AUDIO_BUFFER。
 * @param out_channels 输出通道数。
 * @param out_samples  输出每通道采样数。
 * @param out_sample_rate 输出采样率。
 * @param out_data 输出 PCM 数据指针（交错格式，float32，指向内部缓冲区）。
 * @return 0 成功，非 0 失败。
 */
int oak_value_get_audio_buffer(OakValueHandle value,
                               int* out_channels, int64_t* out_samples,
                               int* out_sample_rate, float** out_data);
```

## 八、序列化

```c
/**
 * @brief 将节点图序列化为 JSON 字符串。
 * @param graph 节点图句柄。
 * @param out_json 输出字符串指针。由 oakcore.so 内部 malloc 分配，调用者必须通过 free() 释放。
 * @return 0 成功，非 0 失败。
 */
int oak_graph_serialize(OakGraphHandle graph, char** out_json);

/**
 * @brief 从 JSON 字符串反序列化节点图。
 * @param graph 现有节点图（会被清空）或 NULL（自动创建新图）。
 * @param json JSON 字符串。
 * @return 若 graph 为 NULL 则返回新创建的图句柄；否则返回传入的 graph。
 * @note 反序列化时会根据 type_id 查询注册表并调用对应工厂创建节点。
 */
OakGraphHandle oak_graph_deserialize(OakGraphHandle graph, const char* json);
```

## 九、回调注册（用于 oakengine.so / oakrenderer）

```c
/**
 * @brief 节点图变更回调签名。
 */
typedef void (*OakGraphChangedFn)(OakGraphHandle graph, void* user_data);

typedef void (*OakNodeAddedFn)(OakGraphHandle graph, OakNodeHandle node, void* user_data);
typedef void (*OakNodeRemovedFn)(OakGraphHandle graph, OakNodeHandle node, void* user_data);
typedef void (*OakConnectionChangedFn)(OakNodeHandle node, const char* input_id, void* user_data);
typedef void (*OakParamChangedFn)(OakNodeHandle node, const char* param_id, void* user_data);

/**
 * @brief 注册节点图事件回调。
 * @param graph 节点图。
 * @param callbacks 回调结构体（传 NULL 表示取消注册）。
 * @param user_data 用户数据指针，会在每次回调时原样传回。
 */
typedef struct {
    OakGraphChangedFn     on_graph_changed;
    OakNodeAddedFn        on_node_added;
    OakNodeRemovedFn      on_node_removed;
    OakConnectionChangedFn on_connection_changed;
    OakParamChangedFn     on_param_changed;
} OakGraphCallbacks;

void oak_graph_set_callbacks(OakGraphHandle graph, const OakGraphCallbacks* callbacks, void* user_data);
```

#ifdef __cplusplus
}
#endif
