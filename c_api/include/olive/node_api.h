/*
 * Olive - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * Pure C API for libolivenode — node graph system.
 */

#ifndef OLIVE_NODE_API_H
#define OLIVE_NODE_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "export.h"
#include "core_api.h"

#define OLIVE_NODE_API_VERSION 1

/* ========== Opaque types ========== */
typedef struct OliveNodeGraph OliveNodeGraph;
typedef struct OliveNode      OliveNode;
typedef struct OliveNodeInput OliveNodeInput;
typedef struct OliveParam     OliveParam;

/* ========== Version ========== */
OLIVE_NODE_API int olive_node_api_version(void);

/* ========== NodeGraph ========== */
OLIVE_NODE_API OliveNodeGraph* olive_node_graph_create(void);
OLIVE_NODE_API void            olive_node_graph_destroy(OliveNodeGraph* graph);
OLIVE_NODE_API OliveNode*      olive_node_graph_add_node(OliveNodeGraph* graph,
                                                           const char* type_id,
                                                           const char* name);
OLIVE_NODE_API OliveNode*      olive_node_graph_find_node(OliveNodeGraph* graph,
                                                            const char* name);
OLIVE_NODE_API int             olive_node_graph_node_count(OliveNodeGraph* graph);

/* ========== Node connections ========== */
OLIVE_NODE_API int olive_node_connect(OliveNode* from_node, int from_output,
                                        OliveNode* to_node, int to_input);
OLIVE_NODE_API int olive_node_disconnect(OliveNode* node, int input_index);

/* ========== Node parameters ========== */
OLIVE_NODE_API int    olive_node_param_set_double(OliveNode* node,
                                                    const char* param_name,
                                                    double value);
OLIVE_NODE_API int    olive_node_param_set_int(OliveNode* node,
                                                 const char* param_name,
                                                 int value);
OLIVE_NODE_API int    olive_node_param_set_rational(OliveNode* node,
                                                      const char* param_name,
                                                      OliveRational value);
OLIVE_NODE_API double olive_node_param_get_double(OliveNode* node,
                                                    const char* param_name);

/* ========== Serialization ========== */
OLIVE_NODE_API char* olive_node_graph_save_xml(OliveNodeGraph* graph,
                                                 size_t* out_len);
OLIVE_NODE_API int   olive_node_graph_load_xml(OliveNodeGraph* graph,
                                                 const char* xml,
                                                 size_t len);
OLIVE_NODE_API void  olive_node_graph_xml_free(char* xml);

#ifdef __cplusplus
}
#endif

#endif // OLIVE_NODE_API_H
