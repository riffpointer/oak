/*
 * Olive - Non-Linear Video Editor
 * Copyright (C) 2025 Oak Video Editor Team
 *
 * C API implementation for libolivenode.
 */

#include "olive/node_api.h"

#include "node/factory.h"
#include "node/node.h"
#include "node/project.h"
#include "node/param.h"

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <cstring>

using olive::Node;
using olive::NodeFactory;
using olive::NodeInput;
using olive::Project;

/* ========== OliveNodeGraph wrapper ========== */

struct OliveNodeGraph {
    Project* project;

    explicit OliveNodeGraph(Project* p)
        : project(p)
    {
    }
};

/* ========== C API implementation ========== */

extern "C" {

/* ----- Version ----- */
int olive_node_api_version(void)
{
    return OLIVE_NODE_API_VERSION;
}

/* ----- NodeGraph ----- */

OliveNodeGraph* olive_node_graph_create(void)
{
    try {
        return new OliveNodeGraph(new Project());
    } catch (...) {
        return nullptr;
    }
}

void olive_node_graph_destroy(OliveNodeGraph* graph)
{
    delete graph;
}

OliveNode* olive_node_graph_add_node(OliveNodeGraph* graph,
                                       const char* type_id,
                                       const char* name)
{
    if (!graph || !graph->project || !type_id) {
        return nullptr;
    }
    try {
        Node* node = NodeFactory::CreateFromID(QString::fromUtf8(type_id));
        if (!node) {
            return nullptr;
        }
        node->setObjectName(QString::fromUtf8(name ? name : type_id));
        node->setParent(graph->project);
        return reinterpret_cast<OliveNode*>(node);
    } catch (...) {
        return nullptr;
    }
}

OliveNode* olive_node_graph_find_node(OliveNodeGraph* graph,
                                        const char* name)
{
    if (!graph || !graph->project || !name) {
        return nullptr;
    }
    try {
        const QVector<Node*>& nodes = graph->project->nodes();
        QString qname = QString::fromUtf8(name);
        for (Node* n : nodes) {
            if (n->objectName() == qname) {
                return reinterpret_cast<OliveNode*>(n);
            }
        }
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

int olive_node_graph_node_count(OliveNodeGraph* graph)
{
    if (!graph || !graph->project) {
        return 0;
    }
    try {
        return graph->project->nodes().size();
    } catch (...) {
        return 0;
    }
}

/* ----- Node connections ----- */

int olive_node_connect(OliveNode* from_node, int from_output,
                         OliveNode* to_node, int to_input)
{
    if (!from_node || !to_node) {
        return OLIVE_ERROR_INVALID;
    }
    try {
        Node* out = reinterpret_cast<Node*>(from_node);
        Node* in = reinterpret_cast<Node*>(to_node);
        const QStringList& inputs = in->inputs();
        if (to_input < 0 || to_input >= inputs.size()) {
            return OLIVE_ERROR_INVALID;
        }
        NodeInput ni(in, inputs[to_input]);
        Node::ConnectEdge(out, ni);
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

int olive_node_disconnect(OliveNode* node, int input_index)
{
    if (!node) {
        return OLIVE_ERROR_INVALID;
    }
    try {
        Node* n = reinterpret_cast<Node*>(node);
        const QStringList& inputs = n->inputs();
        if (input_index < 0 || input_index >= inputs.size()) {
            return OLIVE_ERROR_INVALID;
        }
        NodeInput ni(n, inputs[input_index]);
        Node* output = n->GetConnectedOutput(ni);
        if (output) {
            Node::DisconnectEdge(output, ni);
        }
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

/* ----- Node parameters ----- */

int olive_node_param_set_double(OliveNode* node,
                                  const char* param_name,
                                  double value)
{
    if (!node || !param_name) {
        return OLIVE_ERROR_INVALID;
    }
    try {
        Node* n = reinterpret_cast<Node*>(node);
        n->SetStandardValue(QString::fromUtf8(param_name), QVariant(value));
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

int olive_node_param_set_int(OliveNode* node,
                               const char* param_name,
                               int value)
{
    if (!node || !param_name) {
        return OLIVE_ERROR_INVALID;
    }
    try {
        Node* n = reinterpret_cast<Node*>(node);
        n->SetStandardValue(QString::fromUtf8(param_name), QVariant(value));
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

int olive_node_param_set_rational(OliveNode* node,
                                    const char* param_name,
                                    OliveRational value)
{
    if (!node || !param_name) {
        return OLIVE_ERROR_INVALID;
    }
    try {
        Node* n = reinterpret_cast<Node*>(node);
        n->SetStandardValue(QString::fromUtf8(param_name),
                            QVariant(olive::core::rational((int)value.num, (int)value.den)));
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

double olive_node_param_get_double(OliveNode* node,
                                     const char* param_name)
{
    if (!node || !param_name) {
        return 0.0;
    }
    try {
        Node* n = reinterpret_cast<Node*>(node);
        QVariant v = n->GetStandardValue(QString::fromUtf8(param_name));
        return v.toDouble();
    } catch (...) {
        return 0.0;
    }
}

/* ----- Serialization ----- */

char* olive_node_graph_save_xml(OliveNodeGraph* graph, size_t* out_len)
{
    if (!graph || !graph->project || !out_len) {
        return nullptr;
    }
    try {
        QByteArray data;
        QXmlStreamWriter writer(&data);
        writer.setAutoFormatting(true);
        graph->project->Save(&writer);
        writer.writeEndDocument();

        char* result = static_cast<char*>(std::malloc(data.size() + 1));
        if (!result) {
            return nullptr;
        }
        std::memcpy(result, data.constData(), data.size());
        result[data.size()] = '\0';
        *out_len = data.size();
        return result;
    } catch (...) {
        return nullptr;
    }
}

int olive_node_graph_load_xml(OliveNodeGraph* graph,
                                const char* xml,
                                size_t len)
{
    if (!graph || !graph->project || !xml) {
        return OLIVE_ERROR_INVALID;
    }
    try {
        QByteArray data(xml, static_cast<int>(len));
        QXmlStreamReader reader(data);
        graph->project->Load(&reader);
        return OLIVE_OK;
    } catch (...) {
        return OLIVE_ERROR_GENERIC;
    }
}

void olive_node_graph_xml_free(char* xml)
{
    std::free(xml);
}

} // extern "C"
