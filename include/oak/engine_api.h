/*
 *  oakengine.so C API
 *  节点图加载与渲染服务适配层。
 */

#ifndef OAK_ENGINE_API_H
#define OAK_ENGINE_API_H

#include <stdint.h>
#include <stdbool.h>
#include "oak/frame_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Opaque handles                                                      */
/* ------------------------------------------------------------------ */

typedef struct OakEngineProject* OakEngineProjectHandle;
typedef struct OakEngineSession* OakEngineSessionHandle;

/* ------------------------------------------------------------------ */
/*  Project / NodeGraph lifecycle                                       */
/* ------------------------------------------------------------------ */

OakEngineProjectHandle oak_engine_project_load_xml(const char* xml_str);
void                   oak_engine_project_destroy(OakEngineProjectHandle proj);
int                    oak_engine_project_node_count(OakEngineProjectHandle proj);

/* ------------------------------------------------------------------ */
/*  Render Session                                                      */
/* ------------------------------------------------------------------ */

OakEngineSessionHandle oak_engine_session_create(OakEngineProjectHandle proj,
                                                 int width, int height,
                                                 int pixel_format, /* PixelFormat enum value */
                                                 int64_t timebase_num, int64_t timebase_den);
void                   oak_engine_session_destroy(OakEngineSessionHandle session);

/*
 * Render a single frame at the given time.
 * On success, fills out_frame with frame metadata and returns 0.
 * The pixel data in out_frame->data[0] points to internal memory valid
 * until the next render_frame call or session_destroy.
 */
int                    oak_engine_session_render_frame(OakEngineSessionHandle session,
                                                       int64_t time_num, int64_t time_den,
                                                       OakFrame* out_frame);

#ifdef __cplusplus
}
#endif

#endif /* OAK_ENGINE_API_H */
