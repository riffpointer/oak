#ifndef OAKPLUGINHOST_INTERNAL_H
#define OAKPLUGINHOST_INTERNAL_H

#include "oak/pluginhost_api.h"

#include <string>
#include <vector>

/* Internal C++ structures backing the C API opaque handles.
 * Will wrap existing OpenFX HostSupport classes. */

struct OakPluginHost {
    std::string host_name;
    // TODO: OfxHost, PropertySet, Suite dispatch table
};

struct OakPlugin {
    OakPluginInfo info{};
    // TODO: OfxPlugin*, binary handle
};

struct OakPluginInstance {
    OakPluginHandle plugin = nullptr;
    // TODO: OfxImageEffectHandle, param set, clip instances
    double current_time = 0.0;
    OakPluginTimelineCallbacks timeline_cbs{};
    void* timeline_user_data = nullptr;
};

#endif /* OAKPLUGINHOST_INTERNAL_H */
