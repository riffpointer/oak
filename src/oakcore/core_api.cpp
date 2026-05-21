#include "oak/core_api.h"
#include "oakcore_internal.h"

#include <cstdlib>
#include <cstring>

/* ------------------------------------------------------------------ */
/*  Globals                                                             */
/* ------------------------------------------------------------------ */

static OakNodeTypeRegistry g_registry;

/* ------------------------------------------------------------------ */
/*  Node type registration                                              */
/* ------------------------------------------------------------------ */

int oak_node_register_type(const char* type_id, OakNodeFactoryFn factory) {
    if (!type_id || !factory) return -1;
    if (g_registry.factories.count(type_id)) return -1; /* already exists */
    g_registry.factories[type_id] = factory;
    return 0;
}

void oak_node_unregister_type(const char* type_id) {
    if (type_id) g_registry.factories.erase(type_id);
}

int oak_node_list_types(const char** out_ids, int max_count) {
    if (!out_ids || max_count <= 0) return 0;
    int i = 0;
    for (const auto& kv : g_registry.factories) {
        if (i >= max_count) break;
        out_ids[i++] = kv.first.c_str();
    }
    return i;
}

/* ------------------------------------------------------------------ */
/*  Graph lifecycle                                                     */
/* ------------------------------------------------------------------ */

OakGraphHandle oak_graph_create(void) {
    return new OakNodeGraph();
}

void oak_graph_destroy(OakGraphHandle graph) {
    if (!graph) return;
    // TODO: destroy all nodes inside graph
    delete graph;
}

OakGraphHandle oak_graph_clone(OakGraphHandle graph) {
    if (!graph) return nullptr;
    // TODO: deep copy
    return new OakNodeGraph(*graph);
}

/* ------------------------------------------------------------------ */
/*  Node lifecycle                                                      */
/* ------------------------------------------------------------------ */

OakNodeHandle oak_node_create(const char* type_id) {
    if (!type_id) return nullptr;
    auto it = g_registry.factories.find(type_id);
    if (it == g_registry.factories.end()) return nullptr;
    return it->second();
}

void oak_node_destroy(OakNodeHandle node) {
    if (!node) return;
    delete node;
}

void oak_graph_add_node(OakGraphHandle graph, OakNodeHandle node) {
    if (!graph || !node) return;
    node->graph = graph;
    graph->nodes.push_back(node);
    // TODO: fire callback
}

void oak_graph_remove_node(OakGraphHandle graph, OakNodeHandle node) {
    if (!graph || !node) return;
    // TODO: disconnect all edges, erase from vector
    node->graph = nullptr;
}

OakGraphHandle oak_node_graph(OakNodeHandle node) {
    return node ? node->graph : nullptr;
}

const char* oak_node_type_id(OakNodeHandle node) {
    return node ? node->type_id.c_str() : nullptr;
}

const char* oak_node_label(OakNodeHandle node) {
    return node ? node->label.c_str() : nullptr;
}

void oak_node_set_label(OakNodeHandle node, const char* label) {
    if (node && label) node->label = label;
}

/* ------------------------------------------------------------------ */
/*  Parameter read/write — stubs                                       */
/* ------------------------------------------------------------------ */

int oak_node_param_list(OakNodeHandle node, const char** out_params, int max_count) {
    (void)node; (void)out_params; (void)max_count;
    return 0; /* TODO */
}

OakValueType oak_node_param_type(OakNodeHandle node, const char* param_id) {
    (void)node; (void)param_id;
    return OAK_VALUE_VOID; /* TODO */
}

#define STUB_SETTER(type, suffix) \
    void oak_node_set_##suffix(OakNodeHandle node, const char* param_id, type val) { \
        (void)node; (void)param_id; (void)val; /* TODO */ \
    }
#define STUB_GETTER(type, suffix, default_val) \
    type oak_node_get_##suffix(OakNodeHandle node, const char* param_id) { \
        (void)node; (void)param_id; return default_val; /* TODO */ \
    }

STUB_SETTER(bool, bool)
STUB_GETTER(bool, bool, false)

STUB_SETTER(int, int)
STUB_GETTER(int, int, 0)

STUB_SETTER(int64_t, int64)
STUB_GETTER(int64_t, int64, 0)

STUB_SETTER(float, float)
STUB_GETTER(float, float, 0.0f)

STUB_SETTER(double, double)
STUB_GETTER(double, double, 0.0)

void oak_node_set_rational(OakNodeHandle node, const char* param_id, int64_t num, int64_t den) {
    (void)node; (void)param_id; (void)num; (void)den; /* TODO */
}
void oak_node_get_rational(OakNodeHandle node, const char* param_id, int64_t* out_num, int64_t* out_den) {
    (void)node; (void)param_id;
    if (out_num) *out_num = 0;
    if (out_den) *out_den = 1;
    /* TODO */
}

void        oak_node_set_string(OakNodeHandle node, const char* param_id, const char* val) {
    (void)node; (void)param_id; (void)val; /* TODO */
}
const char* oak_node_get_string(OakNodeHandle node, const char* param_id) {
    (void)node; (void)param_id; return ""; /* TODO */
}

void oak_node_set_vec2 (OakNodeHandle node, const char* param_id, const float* v) {
    (void)node; (void)param_id; (void)v; /* TODO */
}
void oak_node_get_vec2 (OakNodeHandle node, const char* param_id, float* out_v) {
    (void)node; (void)param_id;
    if (out_v) { out_v[0]=0; out_v[1]=0; }
    /* TODO */
}

void oak_node_set_vec3 (OakNodeHandle node, const char* param_id, const float* v) {
    (void)node; (void)param_id; (void)v; /* TODO */
}
void oak_node_get_vec3 (OakNodeHandle node, const char* param_id, float* out_v) {
    (void)node; (void)param_id;
    if (out_v) { out_v[0]=0; out_v[1]=0; out_v[2]=0; }
    /* TODO */
}

void oak_node_set_vec4 (OakNodeHandle node, const char* param_id, const float* v) {
    (void)node; (void)param_id; (void)v; /* TODO */
}
void oak_node_get_vec4 (OakNodeHandle node, const char* param_id, float* out_v) {
    (void)node; (void)param_id;
    if (out_v) { out_v[0]=0; out_v[1]=0; out_v[2]=0; out_v[3]=0; }
    /* TODO */
}

void oak_node_set_color(OakNodeHandle node, const char* param_id, const float* rgba) {
    (void)node; (void)param_id; (void)rgba; /* TODO */
}
void oak_node_get_color(OakNodeHandle node, const char* param_id, float* out_rgba) {
    (void)node; (void)param_id;
    if (out_rgba) { out_rgba[0]=0; out_rgba[1]=0; out_rgba[2]=0; out_rgba[3]=1; }
    /* TODO */
}

void oak_node_set_matrix(OakNodeHandle node, const char* param_id, const float* m16) {
    (void)node; (void)param_id; (void)m16; /* TODO */
}
void oak_node_get_matrix(OakNodeHandle node, const char* param_id, float* out_m16) {
    (void)node; (void)param_id;
    if (out_m16) {
        for (int i=0;i<16;i++) out_m16[i] = (i%5==0) ? 1.0f : 0.0f; /* identity */
    }
    /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Connections — stubs                                                */
/* ------------------------------------------------------------------ */

int oak_node_input_list(OakNodeHandle node, const char** out_ids, int max_count) {
    (void)node; (void)out_ids; (void)max_count;
    return 0; /* TODO */
}

int oak_node_output_list(OakNodeHandle node, const char** out_ids, int max_count) {
    (void)node; (void)out_ids; (void)max_count;
    return 0; /* TODO */
}

int oak_node_input_connection(OakNodeHandle node, const char* input_id,
                              OakNodeHandle* out_source_node,
                              char* out_source_output_id, int source_output_id_capacity) {
    (void)node; (void)input_id;
    if (out_source_node) *out_source_node = nullptr;
    if (out_source_output_id && source_output_id_capacity > 0) out_source_output_id[0] = '\0';
    return 1; /* not connected — TODO */
}

int oak_node_connect(OakNodeHandle out_node, const char* output_id,
                     OakNodeHandle in_node, const char* input_id) {
    (void)out_node; (void)output_id; (void)in_node; (void)input_id;
    return 0; /* TODO */
}

void oak_node_disconnect(OakNodeHandle node, const char* input_id) {
    (void)node; (void)input_id; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Evaluation — stubs                                                 */
/* ------------------------------------------------------------------ */

OakValueHandle oak_node_value_at_time(OakNodeHandle node, const char* output_id,
                                      int64_t time_num, int64_t time_den) {
    (void)node; (void)output_id; (void)time_num; (void)time_den;
    return nullptr; /* TODO */
}

void oak_value_destroy(OakValueHandle value) {
    if (value) delete value;
}

OakValueType oak_value_type(OakValueHandle value) {
    return value ? value->type : OAK_VALUE_VOID;
}

bool    oak_value_get_bool   (OakValueHandle value) { return value ? value->scalar.v_bool : false; }
int     oak_value_get_int    (OakValueHandle value) { return value ? value->scalar.v_int : 0; }
int64_t oak_value_get_int64  (OakValueHandle value) { return value ? value->scalar.v_int64 : 0; }
float   oak_value_get_float  (OakValueHandle value) { return value ? value->scalar.v_float : 0.0f; }
double  oak_value_get_double (OakValueHandle value) { return value ? value->scalar.v_double : 0.0; }

void oak_value_get_rational(OakValueHandle value, int64_t* out_num, int64_t* out_den) {
    (void)value;
    if (out_num) *out_num = 0;
    if (out_den) *out_den = 1;
    /* TODO */
}

const char* oak_value_get_string(OakValueHandle value) {
    return value ? value->v_string.c_str() : "";
}

void oak_value_get_vec2 (OakValueHandle value, float* out_v) {
    (void)value;
    if (out_v) { out_v[0]=0; out_v[1]=0; }
    /* TODO */
}
void oak_value_get_vec3 (OakValueHandle value, float* out_v) {
    (void)value;
    if (out_v) { out_v[0]=0; out_v[1]=0; out_v[2]=0; }
    /* TODO */
}
void oak_value_get_vec4 (OakValueHandle value, float* out_v) {
    (void)value;
    if (out_v) { out_v[0]=0; out_v[1]=0; out_v[2]=0; out_v[3]=0; }
    /* TODO */
}
void oak_value_get_color(OakValueHandle value, float* out_rgba) {
    (void)value;
    if (out_rgba) { out_rgba[0]=0; out_rgba[1]=0; out_rgba[2]=0; out_rgba[3]=1; }
    /* TODO */
}
void oak_value_get_matrix(OakValueHandle value, float* out_m16) {
    (void)value;
    if (out_m16) {
        for (int i=0;i<16;i++) out_m16[i] = (i%5==0) ? 1.0f : 0.0f;
    }
    /* TODO */
}

int oak_value_get_frame(OakValueHandle value,
                        int* out_width, int* out_height, int* out_pix_fmt,
                        void** out_data, int* out_stride) {
    (void)value;
    if (out_width)  *out_width  = 0;
    if (out_height) *out_height = 0;
    if (out_pix_fmt)*out_pix_fmt = 0;
    if (out_data)   *out_data   = nullptr;
    if (out_stride) *out_stride = 0;
    return -1; /* TODO */
}

int oak_value_get_audio_buffer(OakValueHandle value,
                               int* out_channels, int64_t* out_samples,
                               int* out_sample_rate, float** out_data) {
    (void)value;
    if (out_channels)    *out_channels    = 0;
    if (out_samples)     *out_samples     = 0;
    if (out_sample_rate) *out_sample_rate = 0;
    if (out_data)        *out_data        = nullptr;
    return -1; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Serialization — stubs                                              */
/* ------------------------------------------------------------------ */

int oak_graph_serialize(OakGraphHandle graph, char** out_json) {
    (void)graph;
    if (out_json) {
        const char* empty = "{}";
        *out_json = (char*)std::malloc(std::strlen(empty)+1);
        std::strcpy(*out_json, empty);
    }
    return 0; /* TODO */
}

OakGraphHandle oak_graph_deserialize(OakGraphHandle graph, const char* json) {
    (void)json;
    if (!graph) graph = oak_graph_create();
    /* TODO: parse json, create nodes, reconnect */
    return graph;
}

/* ------------------------------------------------------------------ */
/*  Callbacks                                                           */
/* ------------------------------------------------------------------ */

void oak_graph_set_callbacks(OakGraphHandle graph,
                             const OakGraphCallbacks* callbacks,
                             void* user_data) {
    if (!graph) return;
    if (callbacks) {
        graph->callbacks = *callbacks;
    } else {
        graph->callbacks = OakGraphCallbacks{};
    }
    graph->callback_user_data = user_data;
}
