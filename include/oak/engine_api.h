/*
 *  oakengine.so C API
 *  Node graph loading and render session adapter layer.
 *
 *  This API wraps olive::Project, olive::RenderProcessor, and related internals
 *  behind a stable C interface. External modules (including test suites) interact
 *  with the engine only through these functions.
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

/**
 * @brief Load a project from an XML string.
 * @param xml_str UTF-8 encoded XML project string.
 * @return Project handle, or NULL on failure (malformed XML, empty string, etc.).
 * @note The caller owns the returned handle and must release it via
 *       oak_engine_project_destroy.
 */
OakEngineProjectHandle oak_engine_project_load_xml(const char* xml_str);

/**
 * @brief Destroy a project and free all associated resources.
 * @param proj Project handle (NULL is silently ignored).
 * @note After destruction the handle is invalid; do not use or destroy again.
 */
void                   oak_engine_project_destroy(OakEngineProjectHandle proj);

/**
 * @brief Get the number of nodes in the project.
 * @param proj Project handle.
 * @return Node count, or 0 if proj is NULL.
 */
int                    oak_engine_project_node_count(OakEngineProjectHandle proj);

/* ------------------------------------------------------------------ */
/*  Render Session                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a render session for a project.
 * @param proj          Project handle.
 * @param width         Output frame width in pixels.
 * @param height        Output frame height in pixels.
 * @param pixel_format  Pixel format enum value (e.g. PixelFormat::Format cast to int).
 *                      Default pipeline uses RGBA32F.
 * @param timebase_num  Timebase numerator.
 * @param timebase_den  Timebase denominator (must be > 0).
 * @return Session handle, or NULL on failure (null project, zero dimensions,
 *         zero timebase_den, or GPU init failure in headless environments).
 *
 * @note In headless environments without a display/GPU, this function returns
 *       NULL because the renderer requires an OpenGL context.
 */
OakEngineSessionHandle oak_engine_session_create(OakEngineProjectHandle proj,
                                                 int width, int height,
                                                 int pixel_format,
                                                 int64_t timebase_num, int64_t timebase_den);

/**
 * @brief Destroy a render session.
 * @param session Session handle (NULL is silently ignored).
 */
void                   oak_engine_session_destroy(OakEngineSessionHandle session);

/**
 * @brief Render a single frame at the given time.
 * @param session   Session handle.
 * @param time_num  Target time numerator (in session timebase).
 * @param time_den  Target time denominator.
 * @param out_frame Output frame descriptor. On success, filled with frame metadata.
 *                  The pixel data in out_frame->data[0] points to internal memory
 *                  valid until the next render_frame call or session_destroy.
 * @return 0 on success, non-zero on failure.
 *
 * @note The returned frame memory is owned by the session. Callers that need to
 *       keep the data beyond the next render must copy it.
 */
int                    oak_engine_session_render_frame(OakEngineSessionHandle session,
                                                       int64_t time_num, int64_t time_den,
                                                       OakFrame* out_frame);

#ifdef __cplusplus
}
#endif

#endif /* OAK_ENGINE_API_H */
