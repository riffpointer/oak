#ifndef OAKCORE_INTERNAL_H
#define OAKCORE_INTERNAL_H

#include "oak/core_api.h"

#include <string>
#include <vector>
#include <map>
#include <functional>

/* Internal C++ structures backing the C API opaque handles.
 * These will be fleshed out when existing engine/node sources are moved here. */

struct OakNodeTypeRegistry {
    std::map<std::string, OakNodeFactoryFn> factories;
};

struct OakNodeGraph {
    std::vector<OakNodeHandle> nodes;
    OakGraphCallbacks callbacks{};
    void* callback_user_data = nullptr;
    // TODO: connections, parameters, undo/redo state
};

struct OakNode {
    std::string type_id;
    std::string label;
    OakGraphHandle graph = nullptr;
    // TODO: params, inputs, outputs, internal C++ Node* wrapper
};

struct OakNodeInput {
    std::string id;
    OakNodeHandle source_node = nullptr;
    std::string source_output_id;
};

struct OakNodeOutput {
    std::string id;
    // TODO: type, cache
};

struct OakValue {
    OakValueType type = OAK_VALUE_VOID;
    union {
        bool    v_bool;
        int     v_int;
        int64_t v_int64;
        float   v_float;
        double  v_double;
    } scalar;
    std::string v_string;
    float v_vec4[4] = {0};
    float v_matrix[16] = {0};
    // TODO: frame ref, audio buffer ref, custom callbacks
};

#endif /* OAKCORE_INTERNAL_H */
