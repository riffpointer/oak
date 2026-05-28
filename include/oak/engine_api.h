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
#include "oak/core_api.h"

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

/* ------------------------------------------------------------------ */
/*  Engine Lifecycle                                                    */
/* ------------------------------------------------------------------ */

int  oak_engine_init_subsystems(void);
void oak_engine_shutdown_subsystems(void);

/* ------------------------------------------------------------------ */
/*  Project (create / modify / save)                                    */
/* ------------------------------------------------------------------ */

OakEngineProjectHandle oak_project_create(void);
void                   oak_project_destroy(OakEngineProjectHandle h);
void                   oak_project_initialize(OakEngineProjectHandle h);
void                   oak_project_clear(OakEngineProjectHandle h);
void                   oak_project_set_filename(OakEngineProjectHandle h, const char* filename);
const char*            oak_project_filename(OakEngineProjectHandle h);
void                   oak_project_set_modified(OakEngineProjectHandle h, bool modified);
bool                   oak_project_is_modified(OakEngineProjectHandle h);
void                   oak_project_regenerate_uuid(OakEngineProjectHandle h);
const char*            oak_project_get_saved_url(OakEngineProjectHandle h);
int                    oak_project_save(OakEngineProjectHandle h, const char* filename, bool compress, char** out_error);

/* ------------------------------------------------------------------ */
/*  String memory management                                            */
/* ------------------------------------------------------------------ */

void oak_string_free(const char* s);

/* ------------------------------------------------------------------ */
/*  NodeFactory                                                         */
/* ------------------------------------------------------------------ */

int  oak_node_factory_initialize(void);
void oak_node_factory_shutdown(void);

/* ------------------------------------------------------------------ */
/*  ColorManager                                                        */
/* ------------------------------------------------------------------ */

int oak_color_manager_setup_default(void);

/* ------------------------------------------------------------------ */
/*  Config                                                              */
/* ------------------------------------------------------------------ */

int  oak_config_load(void);
void oak_config_save(void);

/* ------------------------------------------------------------------ */
/*  Managers (singleton lifecycle)                                      */
/* ------------------------------------------------------------------ */

int  oak_render_manager_create(void);
void oak_render_manager_destroy(void);
void oak_render_manager_set_project(OakEngineProjectHandle proj);

int  oak_frame_manager_create(void);
void oak_frame_manager_destroy(void);

int  oak_disk_manager_create(void);
void oak_disk_manager_destroy(void);

int  oak_conform_manager_create(void);
void oak_conform_manager_destroy(void);

int  oak_audio_manager_create(void);
void oak_audio_manager_destroy(void);

/* ------------------------------------------------------------------ */
/*  ProjectSerializer                                                   */
/* ------------------------------------------------------------------ */

int  oak_project_serializer_initialize(void);
void oak_project_serializer_destroy(void);

/* ------------------------------------------------------------------ */
/*  UndoStack                                                           */
/* ------------------------------------------------------------------ */

typedef struct OakUndoStack* OakUndoStackHandle;

OakUndoStackHandle oak_undo_stack_create(void);
void               oak_undo_stack_destroy(OakUndoStackHandle stack);
void               oak_undo_stack_push(OakUndoStackHandle stack, void* command_opaque, const char* name);
void               oak_undo_stack_undo(OakUndoStackHandle stack);
void               oak_undo_stack_redo(OakUndoStackHandle stack);
void               oak_undo_stack_clear(OakUndoStackHandle stack);
bool               oak_undo_stack_can_undo(OakUndoStackHandle stack);
bool               oak_undo_stack_can_redo(OakUndoStackHandle stack);

/* ------------------------------------------------------------------ */
/*  UndoCommand                                                         */
/* ------------------------------------------------------------------ */

typedef struct OakUndoCommand* OakUndoCommandHandle;

OakUndoCommandHandle oak_undo_command_multi_create(void);
void oak_undo_command_multi_add_child(OakUndoCommandHandle multi, OakUndoCommandHandle child);
void oak_undo_command_multi_destroy(OakUndoCommandHandle multi);

OakUndoCommandHandle oak_command_node_add_create(OakEngineProjectHandle proj, OakNodeHandle node);
OakUndoCommandHandle oak_command_folder_add_child_create(OakNodeHandle folder, OakNodeHandle child);
OakUndoCommandHandle oak_command_node_set_position_create(OakNodeHandle node, const float* pos);
OakUndoCommandHandle oak_command_node_rename_create(OakNodeHandle node, const char* new_name);

/* ------------------------------------------------------------------ */
/*  Custom callback-based UndoCommand (for app-level commands)         */
/* ------------------------------------------------------------------ */

typedef void (*OakUndoRedoFn)(void* userdata);
typedef void (*OakUndoUndoFn)(void* userdata);

OakUndoCommandHandle oak_undo_command_custom_create(OakUndoRedoFn redo_fn, OakUndoUndoFn undo_fn, void* userdata);

/* ------------------------------------------------------------------ */
/*  Callbacks                                                           */
/* ------------------------------------------------------------------ */

typedef void (*OakProjectModifiedCallback)(OakEngineProjectHandle proj, bool modified, void* userdata);
void oak_project_set_modified_callback(OakEngineProjectHandle proj, OakProjectModifiedCallback cb, void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* OAK_ENGINE_API_H */
