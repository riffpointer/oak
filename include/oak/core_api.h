/*
 *  oakcore.so C API
 *  节点图基础设施：Node/NodeGraph/NodeValue 类型系统、序列化、连接关系。
 */

#ifndef OAK_CORE_API_H
#define OAK_CORE_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Opaque handles                                                      */
/* ------------------------------------------------------------------ */

typedef struct OakNodeGraph* OakGraphHandle;
typedef struct OakNode*      OakNodeHandle;
typedef struct OakNodeInput* OakNodeInputHandle;
typedef struct OakNodeOutput* OakNodeOutputHandle;
typedef struct OakValue*     OakValueHandle;
typedef struct OakNodeTypeRegistry* OakRegistryHandle;

/* ------------------------------------------------------------------ */
/*  Value type enum                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    OAK_VALUE_VOID = 0,
    OAK_VALUE_BOOL,
    OAK_VALUE_INT,
    OAK_VALUE_INT64,
    OAK_VALUE_FLOAT,
    OAK_VALUE_DOUBLE,
    OAK_VALUE_RATIONAL,      /* 分子/分母对 */
    OAK_VALUE_STRING,
    OAK_VALUE_COLOR,         /* RGBA float[4] */
    OAK_VALUE_VEC2,          /* float[2] */
    OAK_VALUE_VEC3,          /* float[3] */
    OAK_VALUE_VEC4,          /* float[4] */
    OAK_VALUE_MATRIX,        /* float[16] 列主序 */
    OAK_VALUE_FRAME,         /* 视频帧（引用计数） */
    OAK_VALUE_AUDIO_BUFFER,  /* 音频缓冲区（引用计数） */
    OAK_VALUE_CUSTOM = 0xFF  /* 节点自定义类型，通过回调访问 */
} OakValueType;

/* ------------------------------------------------------------------ */
/*  Node type registration (for oaknodes.so)                           */
/* ------------------------------------------------------------------ */

typedef OakNodeHandle (*OakNodeFactoryFn)(void);

int  oak_node_register_type(const char* type_id, OakNodeFactoryFn factory);
void oak_node_unregister_type(const char* type_id);
int  oak_node_list_types(const char** out_ids, int max_count);

/* ------------------------------------------------------------------ */
/*  Graph lifecycle                                                     */
/* ------------------------------------------------------------------ */

OakGraphHandle oak_graph_create(void);
void           oak_graph_destroy(OakGraphHandle graph);
OakGraphHandle oak_graph_clone(OakGraphHandle graph);

/* ------------------------------------------------------------------ */
/*  Node lifecycle                                                      */
/* ------------------------------------------------------------------ */

OakNodeHandle  oak_node_create(const char* type_id);
void           oak_node_destroy(OakNodeHandle node);

void           oak_graph_add_node(OakGraphHandle graph, OakNodeHandle node);
void           oak_graph_remove_node(OakGraphHandle graph, OakNodeHandle node);

OakGraphHandle oak_node_graph(OakNodeHandle node);
const char*    oak_node_type_id(OakNodeHandle node);
const char*    oak_node_label(OakNodeHandle node);
void           oak_node_set_label(OakNodeHandle node, const char* label);

/* ------------------------------------------------------------------ */
/*  Parameter read/write                                                */
/* ------------------------------------------------------------------ */

int  oak_node_param_list(OakNodeHandle node, const char** out_params, int max_count);
OakValueType oak_node_param_type(OakNodeHandle node, const char* param_id);

/* ---- scalars ---- */
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

void    oak_node_set_rational(OakNodeHandle node, const char* param_id, int64_t num, int64_t den);
void    oak_node_get_rational(OakNodeHandle node, const char* param_id, int64_t* out_num, int64_t* out_den);

/* ---- string ---- */
void        oak_node_set_string(OakNodeHandle node, const char* param_id, const char* val);
const char* oak_node_get_string(OakNodeHandle node, const char* param_id);

/* ---- vectors / color / matrix ---- */
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

/* ------------------------------------------------------------------ */
/*  Input / output connections                                          */
/* ------------------------------------------------------------------ */

int  oak_node_input_list (OakNodeHandle node, const char** out_ids, int max_count);
int  oak_node_output_list(OakNodeHandle node, const char** out_ids, int max_count);

int  oak_node_input_connection(OakNodeHandle node, const char* input_id,
                               OakNodeHandle* out_source_node,
                               char* out_source_output_id, int source_output_id_capacity);

int  oak_node_connect(OakNodeHandle out_node, const char* output_id,
                      OakNodeHandle in_node,  const char* input_id);
void oak_node_disconnect(OakNodeHandle node, const char* input_id);

/* ------------------------------------------------------------------ */
/*  Evaluation (type-erased value system)                               */
/* ------------------------------------------------------------------ */

OakValueHandle oak_node_value_at_time(OakNodeHandle node, const char* output_id,
                                      int64_t time_num, int64_t time_den);
void           oak_value_destroy(OakValueHandle value);
OakValueType   oak_value_type(OakValueHandle value);

/* ---- scalar extractors ---- */
bool    oak_value_get_bool   (OakValueHandle value);
int     oak_value_get_int    (OakValueHandle value);
int64_t oak_value_get_int64  (OakValueHandle value);
float   oak_value_get_float  (OakValueHandle value);
double  oak_value_get_double (OakValueHandle value);
void    oak_value_get_rational(OakValueHandle value, int64_t* out_num, int64_t* out_den);

const char* oak_value_get_string(OakValueHandle value);

void oak_value_get_vec2 (OakValueHandle value, float* out_v);
void oak_value_get_vec3 (OakValueHandle value, float* out_v);
void oak_value_get_vec4 (OakValueHandle value, float* out_v);
void oak_value_get_color(OakValueHandle value, float* out_rgba);
void oak_value_get_matrix(OakValueHandle value, float* out_m16);

/* ---- media frame extractors (ref-counted, zero-copy) ---- */
int oak_value_get_frame(OakValueHandle value,
                        int* out_width, int* out_height, int* out_pix_fmt,
                        void** out_data, int* out_stride);

int oak_value_get_audio_buffer(OakValueHandle value,
                               int* out_channels, int64_t* out_samples,
                               int* out_sample_rate, float** out_data);

/* ------------------------------------------------------------------ */
/*  Serialization                                                       */
/* ------------------------------------------------------------------ */

int            oak_graph_serialize  (OakGraphHandle graph, char** out_json);
OakGraphHandle oak_graph_deserialize(OakGraphHandle graph, const char* json);

/* ------------------------------------------------------------------ */
/*  Callbacks (for oakengine.so / oakrenderer)                         */
/* ------------------------------------------------------------------ */

typedef void (*OakGraphChangedFn)(OakGraphHandle graph, void* user_data);
typedef void (*OakNodeAddedFn)(OakGraphHandle graph, OakNodeHandle node, void* user_data);
typedef void (*OakNodeRemovedFn)(OakGraphHandle graph, OakNodeHandle node, void* user_data);
typedef void (*OakConnectionChangedFn)(OakNodeHandle node, const char* input_id, void* user_data);
typedef void (*OakParamChangedFn)(OakNodeHandle node, const char* param_id, void* user_data);

typedef struct {
    OakGraphChangedFn      on_graph_changed;
    OakNodeAddedFn         on_node_added;
    OakNodeRemovedFn       on_node_removed;
    OakConnectionChangedFn on_connection_changed;
    OakParamChangedFn      on_param_changed;
} OakGraphCallbacks;

void oak_graph_set_callbacks(OakGraphHandle graph,
                             const OakGraphCallbacks* callbacks,
                             void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* OAK_CORE_API_H */
