/*
 *  oakpluginhost.so C API
 *  OpenFX 插件宿主。对外暴露 oak 特有的宿主注册/查询接口。
 *  OpenFX 标准 Host Suite 见 third_party/openfx/include/ofx*.h
 */

#ifndef OAK_PLUGINHOST_API_H
#define OAK_PLUGINHOST_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Opaque handles                                                      */
/* ------------------------------------------------------------------ */

typedef struct OakPluginHost*     OakPluginHostHandle;
typedef struct OakPlugin*         OakPluginHandle;
typedef struct OakPluginInstance* OakPluginInstanceHandle;

/* ------------------------------------------------------------------ */
/*  Plugin info (POD)                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    const char* id;           /* 插件唯一 ID，如 "com.example.MyBlur" */
    const char* name;         /* 显示名称 */
    const char* version;      /* 版本号 */
    const char* description;  /* 描述 */
    const char* group;        /* 分类组名，如 "Color"、"Filter" */
    int         major_version;
    int         minor_version;
} OakPluginInfo;

/* ------------------------------------------------------------------ */
/*  Host lifecycle                                                      */
/* ------------------------------------------------------------------ */

OakPluginHostHandle oak_plugin_host_create(const char* host_name);
void                oak_plugin_host_destroy(OakPluginHostHandle host);

void oak_plugin_host_set_capability(OakPluginHostHandle host,
                                    const char* capability, int value);

/* ------------------------------------------------------------------ */
/*  Plugin loading & enumeration                                        */
/* ------------------------------------------------------------------ */

int  oak_plugin_load_bundle(OakPluginHostHandle host, const char* bundle_path);
int  oak_plugin_load_from_path(OakPluginHostHandle host, const char* search_path);
int  oak_plugin_count(OakPluginHostHandle host);
int  oak_plugin_get_info(OakPluginHostHandle host, int index, OakPluginInfo* out_info);

OakPluginHandle oak_plugin_find(OakPluginHostHandle host, const char* plugin_id);

/* ------------------------------------------------------------------ */
/*  Plugin instance lifecycle                                           */
/* ------------------------------------------------------------------ */

OakPluginInstanceHandle oak_plugin_instance_create(OakPluginHandle plugin,
                                                   const char* context,
                                                   int project_width, int project_height,
                                                   double pixel_aspect,
                                                   int64_t frame_rate_num, int64_t frame_rate_den);

void   oak_plugin_instance_destroy(OakPluginInstanceHandle instance);
double oak_plugin_instance_time(OakPluginInstanceHandle instance);
void   oak_plugin_instance_set_time(OakPluginInstanceHandle instance, double t);

int    oak_plugin_instance_render(OakPluginInstanceHandle instance,
                                  double time,
                                  double render_scale_x, double render_scale_y,
                                  void** input_textures, int input_count,
                                  void* output_target);

/* ------------------------------------------------------------------ */
/*  Parameter interaction                                               */
/* ------------------------------------------------------------------ */

int oak_plugin_instance_param_count(OakPluginInstanceHandle instance);
int oak_plugin_instance_param_info(OakPluginInstanceHandle instance, int param_index,
                                   const char** out_name, const char** out_type);

/* ---- scalar / string / color parameters ---- */
void        oak_plugin_instance_set_param_double(OakPluginInstanceHandle instance, const char* name, double val);
double      oak_plugin_instance_get_param_double(OakPluginInstanceHandle instance, const char* name);

void        oak_plugin_instance_set_param_int(OakPluginInstanceHandle instance, const char* name, int val);
int         oak_plugin_instance_get_param_int(OakPluginInstanceHandle instance, const char* name);

void        oak_plugin_instance_set_param_string(OakPluginInstanceHandle instance, const char* name, const char* val);
const char* oak_plugin_instance_get_param_string(OakPluginInstanceHandle instance, const char* name);

void        oak_plugin_instance_set_param_bool(OakPluginInstanceHandle instance, const char* name, bool val);
bool        oak_plugin_instance_get_param_bool(OakPluginInstanceHandle instance, const char* name);

void        oak_plugin_instance_set_param_rgb(OakPluginInstanceHandle instance, const char* name, const float* rgb);
void        oak_plugin_instance_get_param_rgb(OakPluginInstanceHandle instance, const char* name, float* out_rgb);

void        oak_plugin_instance_set_param_rgba(OakPluginInstanceHandle instance, const char* name, const float* rgba);
void        oak_plugin_instance_get_param_rgba(OakPluginInstanceHandle instance, const char* name, float* out_rgba);

/* ------------------------------------------------------------------ */
/*  Timeline callbacks (for oakengine.so / oaknodes.so)                */
/* ------------------------------------------------------------------ */

typedef double (*OakPluginGetTimeFn)(void* user_data);
typedef void   (*OakPluginGotoTimeFn)(double t, void* user_data);
typedef void   (*OakPluginGetBoundsFn)(double* out_t1, double* out_t2, void* user_data);

typedef struct {
    OakPluginGetTimeFn  get_time;
    OakPluginGotoTimeFn goto_time;
    OakPluginGetBoundsFn get_bounds;
} OakPluginTimelineCallbacks;

void oak_plugin_instance_set_timeline_callbacks(OakPluginInstanceHandle instance,
                                                const OakPluginTimelineCallbacks* cbs,
                                                void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* OAK_PLUGINHOST_API_H */
