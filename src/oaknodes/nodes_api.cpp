#include "oak/nodes_api.h"
#include "oak/core_api.h"

/* ------------------------------------------------------------------ */
/*  oaknodes.so entry points                                           */
/* ------------------------------------------------------------------ */

// TODO: forward-declare all built-in node factory functions
// extern "C" OakNodeHandle oak_create_footage_node(void);
// extern "C" OakNodeHandle oak_create_sequence_node(void);
// extern "C" OakNodeHandle oak_create_blur_node(void);
// ... etc

int oak_nodes_init(void) {
    // TODO: register all built-in node types via oak_node_register_type()
    // Example:
    // oak_node_register_type("org.oak.Footage",       &oak_create_footage_node);
    // oak_node_register_type("org.oak.Sequence",      &oak_create_sequence_node);
    // oak_node_register_type("org.oak.VideoTransform",&oak_create_videotransform_node);
    // oak_node_register_type("org.oak.Crop",          &oak_create_crop_node);
    // oak_node_register_type("org.oak.Blur",          &oak_create_blur_node);
    // oak_node_register_type("org.oak.ColorCorrection",&oak_create_colorcorrection_node);
    // oak_node_register_type("org.oak.OCIOTransform", &oak_create_ocio_transform_node);
    // oak_node_register_type("org.oak.AudioVolume",   &oak_create_audiovolume_node);
    // oak_node_register_type("org.oak.AudioPan",      &oak_create_audiopan_node);
    // oak_node_register_type("org.oak.Merge",         &oak_create_merge_node);
    // oak_node_register_type("org.oak.TextGenerator", &oak_create_text_node);
    // oak_node_register_type("org.oak.SolidGenerator",&oak_create_solid_node);
    // oak_node_register_type("org.oak.PluginNode",    &oak_create_plugin_node);
    return 0;
}

void oak_nodes_shutdown(void) {
    // TODO: unregister all types registered by this module
}
