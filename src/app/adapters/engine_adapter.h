#ifndef OAK_ENGINE_ADAPTER_H
#define OAK_ENGINE_ADAPTER_H

#include <memory>
#include <QString>
#include <QObject>

// Pure C headers from oakengine (no C++ types, safe to include without linking)
#include "oak/engine_api.h"
#include "oak/core_api.h"
#include "oak/frame_api.h"

namespace olive {
namespace adapters {

class DylibLoader;

/**
 * @brief C++ adapter that loads oakengine.so via dlopen/dlsym
 *
 * All oakengine C API functions are dynamically resolved at runtime.
 * This class wraps them with type-safe C++ methods.
 */
class EngineAdapter {
public:
  EngineAdapter();
  ~EngineAdapter();

  bool Initialize(const QString &lib_path = QString());
  bool IsInitialized() const;

  QString LastError() const;

  // ================================================================
  //  Engine Lifecycle
  // ================================================================
  bool InitSubsystems();
  void ShutdownSubsystems();

  // ================================================================
  //  Project
  // ================================================================
  OakEngineProjectHandle ProjectCreate();
  void ProjectDestroy(OakEngineProjectHandle h);
  void ProjectInitialize(OakEngineProjectHandle h);
  void ProjectClear(OakEngineProjectHandle h);
  void ProjectSetFilename(OakEngineProjectHandle h, const char *filename);
  const char *ProjectFilename(OakEngineProjectHandle h);
  void ProjectSetModified(OakEngineProjectHandle h, bool modified);
  bool ProjectIsModified(OakEngineProjectHandle h);
  void ProjectRegenerateUuid(OakEngineProjectHandle h);
  const char *ProjectGetSavedUrl(OakEngineProjectHandle h);
  OakEngineProjectHandle ProjectLoadFromXml(const char *xml_str);
  int ProjectSave(OakEngineProjectHandle h, const char *filename, bool compress, char **out_error);
  int ProjectNodeCount(OakEngineProjectHandle h);

  // ================================================================
  //  Render Session
  // ================================================================
  OakEngineSessionHandle SessionCreate(OakEngineProjectHandle proj,
                                       int width, int height,
                                       int pixel_format,
                                       int64_t timebase_num, int64_t timebase_den);
  void SessionDestroy(OakEngineSessionHandle session);
  int SessionRenderFrame(OakEngineSessionHandle session,
                         int64_t time_num, int64_t time_den,
                         OakFrame *out_frame);

  // ================================================================
  //  Node / Graph
  // ================================================================
  OakGraphHandle GraphCreate();
  void GraphDestroy(OakGraphHandle graph);
  void GraphAddNode(OakGraphHandle graph, OakNodeHandle node);
  void GraphRemoveNode(OakGraphHandle graph, OakNodeHandle node);
  int GraphSerialize(OakGraphHandle graph, char **out_json);
  OakGraphHandle GraphDeserialize(OakGraphHandle graph, const char *json);

  OakNodeHandle NodeCreate(const char *type_id);
  void NodeDestroy(OakNodeHandle node);
  const char *NodeTypeId(OakNodeHandle node);
  const char *NodeLabel(OakNodeHandle node);
  void NodeSetLabel(OakNodeHandle node, const char *label);

  // Parameter getters/setters
  void NodeSetBool(OakNodeHandle node, const char *param_id, bool val);
  bool NodeGetBool(OakNodeHandle node, const char *param_id);
  void NodeSetInt(OakNodeHandle node, const char *param_id, int val);
  int NodeGetInt(OakNodeHandle node, const char *param_id);
  void NodeSetInt64(OakNodeHandle node, const char *param_id, int64_t val);
  int64_t NodeGetInt64(OakNodeHandle node, const char *param_id);
  void NodeSetFloat(OakNodeHandle node, const char *param_id, float val);
  float NodeGetFloat(OakNodeHandle node, const char *param_id);
  void NodeSetDouble(OakNodeHandle node, const char *param_id, double val);
  double NodeGetDouble(OakNodeHandle node, const char *param_id);
  void NodeSetString(OakNodeHandle node, const char *param_id, const char *val);
  const char *NodeGetString(OakNodeHandle node, const char *param_id);
  void NodeSetRational(OakNodeHandle node, const char *param_id, int64_t num, int64_t den);
  void NodeGetRational(OakNodeHandle node, const char *param_id, int64_t *out_num, int64_t *out_den);
  void NodeSetVec2(OakNodeHandle node, const char *param_id, const float *v);
  void NodeGetVec2(OakNodeHandle node, const char *param_id, float *out_v);
  void NodeSetVec3(OakNodeHandle node, const char *param_id, const float *v);
  void NodeGetVec3(OakNodeHandle node, const char *param_id, float *out_v);
  void NodeSetVec4(OakNodeHandle node, const char *param_id, const float *v);
  void NodeGetVec4(OakNodeHandle node, const char *param_id, float *out_v);
  void NodeSetColor(OakNodeHandle node, const char *param_id, const float *rgba);
  void NodeGetColor(OakNodeHandle node, const char *param_id, float *out_rgba);

  int NodeConnect(OakNodeHandle out_node, const char *output_id,
                  OakNodeHandle in_node, const char *input_id);
  void NodeDisconnect(OakNodeHandle node, const char *input_id);

  // ================================================================
  //  Node Factory
  // ================================================================
  bool NodeFactoryInit();
  void NodeFactoryShutdown();

  // ================================================================
  //  Color Manager
  // ================================================================
  bool ColorManagerSetupDefault();

  // ================================================================
  //  Config
  // ================================================================
  bool ConfigLoad();
  void ConfigSave();

  // ================================================================
  //  Managers
  // ================================================================
  bool RenderManagerCreate();
  void RenderManagerDestroy();
  void RenderManagerSetProject(OakEngineProjectHandle proj);

  bool FrameManagerCreate();
  void FrameManagerDestroy();

  bool DiskManagerCreate();
  void DiskManagerDestroy();

  bool ConformManagerCreate();
  void ConformManagerDestroy();

  bool AudioManagerCreate();
  void AudioManagerDestroy();

  // ================================================================
  //  Serializer
  // ================================================================
  bool ProjectSerializerInit();
  void ProjectSerializerDestroy();

  // ================================================================
  //  Undo
  // ================================================================
  OakUndoStackHandle UndoStackCreate();
  void UndoStackDestroy(OakUndoStackHandle stack);
  void UndoStackPush(OakUndoStackHandle stack, OakUndoCommandHandle cmd, const char *name);
  void UndoStackUndo(OakUndoStackHandle stack);
  void UndoStackRedo(OakUndoStackHandle stack);
  void UndoStackClear(OakUndoStackHandle stack);
  bool UndoStackCanUndo(OakUndoStackHandle stack);
  bool UndoStackCanRedo(OakUndoStackHandle stack);

  OakUndoCommandHandle UndoCommandMultiCreate();
  void UndoCommandMultiAddChild(OakUndoCommandHandle multi, OakUndoCommandHandle child);
  void UndoCommandMultiDestroy(OakUndoCommandHandle multi);

  OakUndoCommandHandle CommandNodeAddCreate(OakEngineProjectHandle proj, OakNodeHandle node);
  OakUndoCommandHandle CommandFolderAddChildCreate(OakNodeHandle folder, OakNodeHandle child);
  OakUndoCommandHandle CommandNodeSetPositionCreate(OakNodeHandle node, const float *pos);
  OakUndoCommandHandle CommandNodeRenameCreate(OakNodeHandle node, const char *new_name);

  // Custom callback-based command
  OakUndoCommandHandle CustomUndoCommandCreate(void (*redo_fn)(void*), void (*undo_fn)(void*), void *userdata);

  // ================================================================
  //  Callbacks
  // ================================================================
  void ProjectSetModifiedCallback(OakEngineProjectHandle proj,
                                  void (*callback)(OakEngineProjectHandle, bool, void *),
                                  void *userdata);

  // ================================================================
  //  String cleanup
  // ================================================================
  void StringFree(const char *s);

private:
  std::unique_ptr<DylibLoader> loader_;
  QString last_error_;

  // ---- Function pointer types ----
  using fn_engine_init_subsystems_t = int(*)();
  using fn_engine_shutdown_subsystems_t = void(*)();

  using fn_project_create_t = OakEngineProjectHandle(*)();
  using fn_project_destroy_t = void(*)(OakEngineProjectHandle);
  using fn_project_initialize_t = void(*)(OakEngineProjectHandle);
  using fn_project_clear_t = void(*)(OakEngineProjectHandle);
  using fn_project_set_filename_t = void(*)(OakEngineProjectHandle, const char*);
  using fn_project_filename_t = const char*(*)(OakEngineProjectHandle);
  using fn_project_set_modified_t = void(*)(OakEngineProjectHandle, bool);
  using fn_project_is_modified_t = bool(*)(OakEngineProjectHandle);
  using fn_project_regenerate_uuid_t = void(*)(OakEngineProjectHandle);
  using fn_project_get_saved_url_t = const char*(*)(OakEngineProjectHandle);
  using fn_project_save_t = int(*)(OakEngineProjectHandle, const char*, bool, char**);

  using fn_node_factory_init_t = int(*)();
  using fn_node_factory_shutdown_t = void(*)();

  using fn_color_manager_setup_default_t = int(*)();

  using fn_config_load_t = int(*)();
  using fn_config_save_t = void(*)();

  using fn_render_manager_create_t = int(*)();
  using fn_render_manager_destroy_t = void(*)();
  using fn_render_manager_set_project_t = void(*)(OakEngineProjectHandle);

  using fn_frame_manager_create_t = int(*)();
  using fn_frame_manager_destroy_t = void(*)();

  using fn_disk_manager_create_t = int(*)();
  using fn_disk_manager_destroy_t = void(*)();

  using fn_conform_manager_create_t = int(*)();
  using fn_conform_manager_destroy_t = void(*)();

  using fn_audio_manager_create_t = int(*)();
  using fn_audio_manager_destroy_t = void(*)();

  using fn_project_serializer_init_t = int(*)();
  using fn_project_serializer_destroy_t = void(*)();

  using fn_undo_stack_create_t = OakUndoStackHandle(*)();
  using fn_undo_stack_destroy_t = void(*)(OakUndoStackHandle);
  using fn_undo_stack_push_t = void(*)(OakUndoStackHandle, OakUndoCommandHandle, const char*);
  using fn_undo_stack_undo_t = void(*)(OakUndoStackHandle);
  using fn_undo_stack_redo_t = void(*)(OakUndoStackHandle);
  using fn_undo_stack_clear_t = void(*)(OakUndoStackHandle);
  using fn_undo_stack_can_undo_t = bool(*)(OakUndoStackHandle);
  using fn_undo_stack_can_redo_t = bool(*)(OakUndoStackHandle);

  using fn_undo_command_multi_create_t = OakUndoCommandHandle(*)();
  using fn_undo_command_multi_add_child_t = void(*)(OakUndoCommandHandle, OakUndoCommandHandle);
  using fn_undo_command_multi_destroy_t = void(*)(OakUndoCommandHandle);

  using fn_command_node_add_create_t = OakUndoCommandHandle(*)(OakEngineProjectHandle, OakNodeHandle);
  using fn_command_folder_add_child_create_t = OakUndoCommandHandle(*)(OakNodeHandle, OakNodeHandle);
  using fn_command_node_set_position_create_t = OakUndoCommandHandle(*)(OakNodeHandle, const float*);
  using fn_command_node_rename_create_t = OakUndoCommandHandle(*)(OakNodeHandle, const char*);
  using fn_undo_command_custom_create_t = OakUndoCommandHandle(*)(void(*)(void*), void(*)(void*), void*);

  using fn_project_set_modified_callback_t = void(*)(OakEngineProjectHandle, void(*)(OakEngineProjectHandle, bool, void*), void*);

  using fn_string_free_t = void(*)(const char*);

  // ---- Function pointers ----
  fn_engine_init_subsystems_t fn_engine_init_subsystems_ = nullptr;
  fn_engine_shutdown_subsystems_t fn_engine_shutdown_subsystems_ = nullptr;

  fn_project_create_t fn_project_create_ = nullptr;
  fn_project_destroy_t fn_project_destroy_ = nullptr;
  fn_project_initialize_t fn_project_initialize_ = nullptr;
  fn_project_clear_t fn_project_clear_ = nullptr;
  fn_project_set_filename_t fn_project_set_filename_ = nullptr;
  fn_project_filename_t fn_project_filename_ = nullptr;
  fn_project_set_modified_t fn_project_set_modified_ = nullptr;
  fn_project_is_modified_t fn_project_is_modified_ = nullptr;
  fn_project_regenerate_uuid_t fn_project_regenerate_uuid_ = nullptr;
  fn_project_get_saved_url_t fn_project_get_saved_url_ = nullptr;
  fn_project_save_t fn_project_save_ = nullptr;

  fn_node_factory_init_t fn_node_factory_init_ = nullptr;
  fn_node_factory_shutdown_t fn_node_factory_shutdown_ = nullptr;

  fn_color_manager_setup_default_t fn_color_manager_setup_default_ = nullptr;

  fn_config_load_t fn_config_load_ = nullptr;
  fn_config_save_t fn_config_save_ = nullptr;

  fn_render_manager_create_t fn_render_manager_create_ = nullptr;
  fn_render_manager_destroy_t fn_render_manager_destroy_ = nullptr;
  fn_render_manager_set_project_t fn_render_manager_set_project_ = nullptr;

  fn_frame_manager_create_t fn_frame_manager_create_ = nullptr;
  fn_frame_manager_destroy_t fn_frame_manager_destroy_ = nullptr;

  fn_disk_manager_create_t fn_disk_manager_create_ = nullptr;
  fn_disk_manager_destroy_t fn_disk_manager_destroy_ = nullptr;

  fn_conform_manager_create_t fn_conform_manager_create_ = nullptr;
  fn_conform_manager_destroy_t fn_conform_manager_destroy_ = nullptr;

  fn_audio_manager_create_t fn_audio_manager_create_ = nullptr;
  fn_audio_manager_destroy_t fn_audio_manager_destroy_ = nullptr;

  fn_project_serializer_init_t fn_project_serializer_init_ = nullptr;
  fn_project_serializer_destroy_t fn_project_serializer_destroy_ = nullptr;

  fn_undo_stack_create_t fn_undo_stack_create_ = nullptr;
  fn_undo_stack_destroy_t fn_undo_stack_destroy_ = nullptr;
  fn_undo_stack_push_t fn_undo_stack_push_ = nullptr;
  fn_undo_stack_undo_t fn_undo_stack_undo_ = nullptr;
  fn_undo_stack_redo_t fn_undo_stack_redo_ = nullptr;
  fn_undo_stack_clear_t fn_undo_stack_clear_ = nullptr;
  fn_undo_stack_can_undo_t fn_undo_stack_can_undo_ = nullptr;
  fn_undo_stack_can_redo_t fn_undo_stack_can_redo_ = nullptr;

  fn_undo_command_multi_create_t fn_undo_command_multi_create_ = nullptr;
  fn_undo_command_multi_add_child_t fn_undo_command_multi_add_child_ = nullptr;
  fn_undo_command_multi_destroy_t fn_undo_command_multi_destroy_ = nullptr;

  fn_command_node_add_create_t fn_command_node_add_create_ = nullptr;
  fn_command_folder_add_child_create_t fn_command_folder_add_child_create_ = nullptr;
  fn_command_node_set_position_create_t fn_command_node_set_position_create_ = nullptr;
  fn_command_node_rename_create_t fn_command_node_rename_create_ = nullptr;
  fn_undo_command_custom_create_t fn_undo_command_custom_create_ = nullptr;

  fn_project_set_modified_callback_t fn_project_set_modified_callback_ = nullptr;

  fn_string_free_t fn_string_free_ = nullptr;

  bool ResolveAll();
};

/**
 * @brief Global engine adapter singleton
 */
EngineAdapter *GetEngineAdapter();

} // namespace adapters
} // namespace olive

#endif // OAK_ENGINE_ADAPTER_H
