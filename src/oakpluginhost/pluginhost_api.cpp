#include "oak/pluginhost_api.h"
#include "oakpluginhost_internal.h"

#include <cstdlib>
#include <cstring>

/* ------------------------------------------------------------------ */
/*  Host lifecycle                                                      */
/* ------------------------------------------------------------------ */

OakPluginHostHandle oak_plugin_host_create(const char* host_name) {
    auto* h = new OakPluginHost();
    h->host_name = host_name ? host_name : "OakVideoEditor";
    // TODO: init OfxHost, register standard suites
    return h;
}

void oak_plugin_host_destroy(OakPluginHostHandle host) {
    if (!host) return;
    // TODO: unload all plugins, close bundles
    delete host;
}

void oak_plugin_host_set_capability(OakPluginHostHandle host,
                                    const char* capability, int value) {
    (void)host; (void)capability; (void)value;
    // TODO: store in host property set
}

/* ------------------------------------------------------------------ */
/*  Plugin loading & enumeration                                        */
/* ------------------------------------------------------------------ */

int oak_plugin_load_bundle(OakPluginHostHandle host, const char* bundle_path) {
    (void)host; (void)bundle_path;
    return -1; /* TODO */
}

int oak_plugin_load_from_path(OakPluginHostHandle host, const char* search_path) {
    (void)host; (void)search_path;
    return 0; /* TODO */
}

int oak_plugin_count(OakPluginHostHandle host) {
    (void)host;
    return 0; /* TODO */
}

int oak_plugin_get_info(OakPluginHostHandle host, int index, OakPluginInfo* out_info) {
    (void)host; (void)index;
    if (out_info) std::memset(out_info, 0, sizeof(*out_info));
    return -1; /* TODO */
}

OakPluginHandle oak_plugin_find(OakPluginHostHandle host, const char* plugin_id) {
    (void)host; (void)plugin_id;
    return nullptr; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Plugin instance lifecycle                                           */
/* ------------------------------------------------------------------ */

OakPluginInstanceHandle oak_plugin_instance_create(OakPluginHandle plugin,
                                                   const char* context,
                                                   int project_width, int project_height,
                                                   double pixel_aspect,
                                                   int64_t frame_rate_num, int64_t frame_rate_den) {
    (void)plugin; (void)context; (void)project_width; (void)project_height;
    (void)pixel_aspect; (void)frame_rate_num; (void)frame_rate_den;
    return nullptr; /* TODO */
}

void oak_plugin_instance_destroy(OakPluginInstanceHandle instance) {
    delete instance;
}

double oak_plugin_instance_time(OakPluginInstanceHandle instance) {
    return instance ? instance->current_time : 0.0;
}

void oak_plugin_instance_set_time(OakPluginInstanceHandle instance, double t) {
    if (instance) instance->current_time = t;
    // TODO: trigger instanceChangedAction if time changed
}

int oak_plugin_instance_render(OakPluginInstanceHandle instance,
                               double time,
                               double render_scale_x, double render_scale_y,
                               void** input_textures, int input_count,
                               void* output_target) {
    (void)instance; (void)time; (void)render_scale_x; (void)render_scale_y;
    (void)input_textures; (void)input_count; (void)output_target;
    return -1; /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Parameter interaction                                               */
/* ------------------------------------------------------------------ */

int oak_plugin_instance_param_count(OakPluginInstanceHandle instance) {
    (void)instance;
    return 0; /* TODO */
}

int oak_plugin_instance_param_info(OakPluginInstanceHandle instance, int param_index,
                                   const char** out_name, const char** out_type) {
    (void)instance; (void)param_index;
    if (out_name) *out_name = nullptr;
    if (out_type) *out_type = nullptr;
    return -1; /* TODO */
}

void        oak_plugin_instance_set_param_double(OakPluginInstanceHandle instance, const char* name, double val) {
    (void)instance; (void)name; (void)val; /* TODO */
}
double      oak_plugin_instance_get_param_double(OakPluginInstanceHandle instance, const char* name) {
    (void)instance; (void)name; return 0.0; /* TODO */
}

void        oak_plugin_instance_set_param_int(OakPluginInstanceHandle instance, const char* name, int val) {
    (void)instance; (void)name; (void)val; /* TODO */
}
int         oak_plugin_instance_get_param_int(OakPluginInstanceHandle instance, const char* name) {
    (void)instance; (void)name; return 0; /* TODO */
}

void        oak_plugin_instance_set_param_string(OakPluginInstanceHandle instance, const char* name, const char* val) {
    (void)instance; (void)name; (void)val; /* TODO */
}
const char* oak_plugin_instance_get_param_string(OakPluginInstanceHandle instance, const char* name) {
    (void)instance; (void)name; return ""; /* TODO */
}

void        oak_plugin_instance_set_param_bool(OakPluginInstanceHandle instance, const char* name, bool val) {
    (void)instance; (void)name; (void)val; /* TODO */
}
bool        oak_plugin_instance_get_param_bool(OakPluginInstanceHandle instance, const char* name) {
    (void)instance; (void)name; return false; /* TODO */
}

void        oak_plugin_instance_set_param_rgb(OakPluginInstanceHandle instance, const char* name, const float* rgb) {
    (void)instance; (void)name; (void)rgb; /* TODO */
}
void        oak_plugin_instance_get_param_rgb(OakPluginInstanceHandle instance, const char* name, float* out_rgb) {
    (void)instance; (void)name;
    if (out_rgb) { out_rgb[0]=0; out_rgb[1]=0; out_rgb[2]=0; }
    /* TODO */
}

void        oak_plugin_instance_set_param_rgba(OakPluginInstanceHandle instance, const char* name, const float* rgba) {
    (void)instance; (void)name; (void)rgba; /* TODO */
}
void        oak_plugin_instance_get_param_rgba(OakPluginInstanceHandle instance, const char* name, float* out_rgba) {
    (void)instance; (void)name;
    if (out_rgba) { out_rgba[0]=0; out_rgba[1]=0; out_rgba[2]=0; out_rgba[3]=1; }
    /* TODO */
}

/* ------------------------------------------------------------------ */
/*  Timeline callbacks                                                  */
/* ------------------------------------------------------------------ */

void oak_plugin_instance_set_timeline_callbacks(OakPluginInstanceHandle instance,
                                                const OakPluginTimelineCallbacks* cbs,
                                                void* user_data) {
    if (!instance) return;
    if (cbs) {
        instance->timeline_cbs = *cbs;
    } else {
        instance->timeline_cbs = OakPluginTimelineCallbacks{};
    }
    instance->timeline_user_data = user_data;
}
