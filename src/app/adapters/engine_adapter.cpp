#include "engine_adapter.h"
#include "dlopen_helper.h"

#include <QCoreApplication>
#include <QDir>

namespace olive {
namespace adapters {

static EngineAdapter *g_engine_adapter = nullptr;

EngineAdapter *GetEngineAdapter()
{
  if (!g_engine_adapter) {
    g_engine_adapter = new EngineAdapter();
  }
  return g_engine_adapter;
}

EngineAdapter::EngineAdapter()
  : loader_(std::make_unique<DylibLoader>())
{
}

EngineAdapter::~EngineAdapter()
{
  if (g_engine_adapter == this) {
    g_engine_adapter = nullptr;
  }
}

bool EngineAdapter::Initialize(const QString &lib_path)
{
  QString path = lib_path;
  if (path.isEmpty()) {
    // Try to find oakengine in standard locations relative to executable
    QString app_dir = QCoreApplication::applicationDirPath();
#ifdef __APPLE__
    // macOS bundle: Olive.app/Contents/MacOS/../Frameworks or ../lib
    QStringList candidates = {
      app_dir + "/../lib/liboakengine.dylib",
      app_dir + "/../Frameworks/liboakengine.dylib",
      app_dir + "/liboakengine.dylib",
      QStringLiteral("liboakengine.dylib")
    };
#else
    QStringList candidates = {
      app_dir + "/lib/liboakengine.so",
      app_dir + "/liboakengine.so",
      QStringLiteral("liboakengine.so")
    };
#endif
    for (const QString &c : candidates) {
      if (QFile::exists(c)) {
        path = c;
        break;
      }
    }
  }

  if (path.isEmpty()) {
    last_error_ = QStringLiteral("Could not find oakengine library");
    return false;
  }

  if (!loader_->Load(path)) {
    last_error_ = loader_->LastError();
    return false;
  }

  if (!ResolveAll()) {
    loader_->Unload();
    return false;
  }

  return true;
}

bool EngineAdapter::IsInitialized() const
{
  return loader_->IsLoaded();
}

QString EngineAdapter::LastError() const
{
  return last_error_;
}

bool EngineAdapter::ResolveAll()
{
  // Lifecycle
  fn_engine_init_subsystems_ = loader_->Resolve<fn_engine_init_subsystems_t>("oak_engine_init_subsystems");
  fn_engine_shutdown_subsystems_ = loader_->Resolve<fn_engine_shutdown_subsystems_t>("oak_engine_shutdown_subsystems");

  // Project
  fn_project_create_ = loader_->Resolve<fn_project_create_t>("oak_project_create");
  fn_project_destroy_ = loader_->Resolve<fn_project_destroy_t>("oak_project_destroy");
  fn_project_initialize_ = loader_->Resolve<fn_project_initialize_t>("oak_project_initialize");
  fn_project_clear_ = loader_->Resolve<fn_project_clear_t>("oak_project_clear");
  fn_project_set_filename_ = loader_->Resolve<fn_project_set_filename_t>("oak_project_set_filename");
  fn_project_filename_ = loader_->Resolve<fn_project_filename_t>("oak_project_filename");
  fn_project_set_modified_ = loader_->Resolve<fn_project_set_modified_t>("oak_project_set_modified");
  fn_project_is_modified_ = loader_->Resolve<fn_project_is_modified_t>("oak_project_is_modified");
  fn_project_regenerate_uuid_ = loader_->Resolve<fn_project_regenerate_uuid_t>("oak_project_regenerate_uuid");
  fn_project_get_saved_url_ = loader_->Resolve<fn_project_get_saved_url_t>("oak_project_get_saved_url");
  fn_project_save_ = loader_->Resolve<fn_project_save_t>("oak_project_save");

  // NodeFactory
  fn_node_factory_init_ = loader_->Resolve<fn_node_factory_init_t>("oak_node_factory_initialize");
  fn_node_factory_shutdown_ = loader_->Resolve<fn_node_factory_shutdown_t>("oak_node_factory_shutdown");

  // ColorManager
  fn_color_manager_setup_default_ = loader_->Resolve<fn_color_manager_setup_default_t>("oak_color_manager_setup_default");

  // Config
  fn_config_load_ = loader_->Resolve<fn_config_load_t>("oak_config_load");
  fn_config_save_ = loader_->Resolve<fn_config_save_t>("oak_config_save");

  // Managers
  fn_render_manager_create_ = loader_->Resolve<fn_render_manager_create_t>("oak_render_manager_create");
  fn_render_manager_destroy_ = loader_->Resolve<fn_render_manager_destroy_t>("oak_render_manager_destroy");
  fn_render_manager_set_project_ = loader_->Resolve<fn_render_manager_set_project_t>("oak_render_manager_set_project");

  fn_frame_manager_create_ = loader_->Resolve<fn_frame_manager_create_t>("oak_frame_manager_create");
  fn_frame_manager_destroy_ = loader_->Resolve<fn_frame_manager_destroy_t>("oak_frame_manager_destroy");

  fn_disk_manager_create_ = loader_->Resolve<fn_disk_manager_create_t>("oak_disk_manager_create");
  fn_disk_manager_destroy_ = loader_->Resolve<fn_disk_manager_destroy_t>("oak_disk_manager_destroy");

  fn_conform_manager_create_ = loader_->Resolve<fn_conform_manager_create_t>("oak_conform_manager_create");
  fn_conform_manager_destroy_ = loader_->Resolve<fn_conform_manager_destroy_t>("oak_conform_manager_destroy");

  fn_audio_manager_create_ = loader_->Resolve<fn_audio_manager_create_t>("oak_audio_manager_create");
  fn_audio_manager_destroy_ = loader_->Resolve<fn_audio_manager_destroy_t>("oak_audio_manager_destroy");

  // Serializer
  fn_project_serializer_init_ = loader_->Resolve<fn_project_serializer_init_t>("oak_project_serializer_initialize");
  fn_project_serializer_destroy_ = loader_->Resolve<fn_project_serializer_destroy_t>("oak_project_serializer_destroy");

  // Undo
  fn_undo_stack_create_ = loader_->Resolve<fn_undo_stack_create_t>("oak_undo_stack_create");
  fn_undo_stack_destroy_ = loader_->Resolve<fn_undo_stack_destroy_t>("oak_undo_stack_destroy");
  fn_undo_stack_push_ = loader_->Resolve<fn_undo_stack_push_t>("oak_undo_stack_push");
  fn_undo_stack_undo_ = loader_->Resolve<fn_undo_stack_undo_t>("oak_undo_stack_undo");
  fn_undo_stack_redo_ = loader_->Resolve<fn_undo_stack_redo_t>("oak_undo_stack_redo");
  fn_undo_stack_clear_ = loader_->Resolve<fn_undo_stack_clear_t>("oak_undo_stack_clear");
  fn_undo_stack_can_undo_ = loader_->Resolve<fn_undo_stack_can_undo_t>("oak_undo_stack_can_undo");
  fn_undo_stack_can_redo_ = loader_->Resolve<fn_undo_stack_can_redo_t>("oak_undo_stack_can_redo");

  fn_undo_command_multi_create_ = loader_->Resolve<fn_undo_command_multi_create_t>("oak_undo_command_multi_create");
  fn_undo_command_multi_add_child_ = loader_->Resolve<fn_undo_command_multi_add_child_t>("oak_undo_command_multi_add_child");
  fn_undo_command_multi_destroy_ = loader_->Resolve<fn_undo_command_multi_destroy_t>("oak_undo_command_multi_destroy");

  fn_command_node_add_create_ = loader_->Resolve<fn_command_node_add_create_t>("oak_command_node_add_create");
  fn_command_folder_add_child_create_ = loader_->Resolve<fn_command_folder_add_child_create_t>("oak_command_folder_add_child_create");
  fn_command_node_set_position_create_ = loader_->Resolve<fn_command_node_set_position_create_t>("oak_command_node_set_position_create");
  fn_command_node_rename_create_ = loader_->Resolve<fn_command_node_rename_create_t>("oak_command_node_rename_create");
  fn_undo_command_custom_create_ = loader_->Resolve<fn_undo_command_custom_create_t>("oak_undo_command_custom_create");

  // Callbacks
  fn_project_set_modified_callback_ = loader_->Resolve<fn_project_set_modified_callback_t>("oak_project_set_modified_callback");

  // String
  fn_string_free_ = loader_->Resolve<fn_string_free_t>("oak_string_free");

  // Check critical functions
  if (!fn_engine_init_subsystems_ || !fn_project_create_) {
    last_error_ = QStringLiteral("Failed to resolve critical oakengine symbols");
    return false;
  }

  return true;
}

// ================================================================
//  Engine Lifecycle
// ================================================================
bool EngineAdapter::InitSubsystems()
{
  if (!fn_engine_init_subsystems_) return false;
  return fn_engine_init_subsystems_() == 0;
}

void EngineAdapter::ShutdownSubsystems()
{
  if (fn_engine_shutdown_subsystems_) fn_engine_shutdown_subsystems_();
}

// ================================================================
//  Project
// ================================================================
OakEngineProjectHandle EngineAdapter::ProjectCreate()
{
  if (!fn_project_create_) return nullptr;
  return fn_project_create_();
}

void EngineAdapter::ProjectDestroy(OakEngineProjectHandle h)
{
  if (fn_project_destroy_) fn_project_destroy_(h);
}

void EngineAdapter::ProjectInitialize(OakEngineProjectHandle h)
{
  if (fn_project_initialize_) fn_project_initialize_(h);
}

void EngineAdapter::ProjectClear(OakEngineProjectHandle h)
{
  if (fn_project_clear_) fn_project_clear_(h);
}

void EngineAdapter::ProjectSetFilename(OakEngineProjectHandle h, const char *filename)
{
  if (fn_project_set_filename_) fn_project_set_filename_(h, filename);
}

const char *EngineAdapter::ProjectFilename(OakEngineProjectHandle h)
{
  if (!fn_project_filename_) return "";
  return fn_project_filename_(h);
}

void EngineAdapter::ProjectSetModified(OakEngineProjectHandle h, bool modified)
{
  if (fn_project_set_modified_) fn_project_set_modified_(h, modified);
}

bool EngineAdapter::ProjectIsModified(OakEngineProjectHandle h)
{
  if (!fn_project_is_modified_) return false;
  return fn_project_is_modified_(h);
}

void EngineAdapter::ProjectRegenerateUuid(OakEngineProjectHandle h)
{
  if (fn_project_regenerate_uuid_) fn_project_regenerate_uuid_(h);
}

const char *EngineAdapter::ProjectGetSavedUrl(OakEngineProjectHandle h)
{
  if (!fn_project_get_saved_url_) return "";
  return fn_project_get_saved_url_(h);
}

OakEngineProjectHandle EngineAdapter::ProjectLoadFromXml(const char *xml_str)
{
  auto fn = loader_->Resolve< OakEngineProjectHandle(*)(const char*) >("oak_engine_project_load_xml");
  if (!fn) return nullptr;
  return fn(xml_str);
}

int EngineAdapter::ProjectSave(OakEngineProjectHandle h, const char *filename, bool compress, char **out_error)
{
  if (!fn_project_save_) return -1;
  return fn_project_save_(h, filename, compress, out_error);
}

int EngineAdapter::ProjectNodeCount(OakEngineProjectHandle h)
{
  auto fn = loader_->Resolve< int(*)(OakEngineProjectHandle) >("oak_engine_project_node_count");
  if (!fn) return 0;
  return fn(h);
}

// ================================================================
//  Render Session
// ================================================================
OakEngineSessionHandle EngineAdapter::SessionCreate(OakEngineProjectHandle proj,
                                                     int width, int height,
                                                     int pixel_format,
                                                     int64_t timebase_num, int64_t timebase_den)
{
  auto fn = loader_->Resolve< OakEngineSessionHandle(*)(OakEngineProjectHandle, int, int, int, int64_t, int64_t) >("oak_engine_session_create");
  if (!fn) return nullptr;
  return fn(proj, width, height, pixel_format, timebase_num, timebase_den);
}

void EngineAdapter::SessionDestroy(OakEngineSessionHandle session)
{
  auto fn = loader_->Resolve< void(*)(OakEngineSessionHandle) >("oak_engine_session_destroy");
  if (fn) fn(session);
}

int EngineAdapter::SessionRenderFrame(OakEngineSessionHandle session,
                                       int64_t time_num, int64_t time_den,
                                       OakFrame *out_frame)
{
  auto fn = loader_->Resolve< int(*)(OakEngineSessionHandle, int64_t, int64_t, OakFrame*) >("oak_engine_session_render_frame");
  if (!fn) return -1;
  return fn(session, time_num, time_den, out_frame);
}

// ================================================================
//  Node / Graph  (forward to core_api symbols in oakengine)
// ================================================================
OakGraphHandle EngineAdapter::GraphCreate()
{
  auto fn = loader_->Resolve< OakGraphHandle(*)() >("oak_graph_create");
  if (!fn) return nullptr;
  return fn();
}

void EngineAdapter::GraphDestroy(OakGraphHandle graph)
{
  auto fn = loader_->Resolve< void(*)(OakGraphHandle) >("oak_graph_destroy");
  if (fn) fn(graph);
}

void EngineAdapter::GraphAddNode(OakGraphHandle graph, OakNodeHandle node)
{
  auto fn = loader_->Resolve< void(*)(OakGraphHandle, OakNodeHandle) >("oak_graph_add_node");
  if (fn) fn(graph, node);
}

void EngineAdapter::GraphRemoveNode(OakGraphHandle graph, OakNodeHandle node)
{
  auto fn = loader_->Resolve< void(*)(OakGraphHandle, OakNodeHandle) >("oak_graph_remove_node");
  if (fn) fn(graph, node);
}

int EngineAdapter::GraphSerialize(OakGraphHandle graph, char **out_json)
{
  auto fn = loader_->Resolve< int(*)(OakGraphHandle, char**) >("oak_graph_serialize");
  if (!fn) return -1;
  return fn(graph, out_json);
}

OakGraphHandle EngineAdapter::GraphDeserialize(OakGraphHandle graph, const char *json)
{
  auto fn = loader_->Resolve< OakGraphHandle(*)(OakGraphHandle, const char*) >("oak_graph_deserialize");
  if (!fn) return nullptr;
  return fn(graph, json);
}

OakNodeHandle EngineAdapter::NodeCreate(const char *type_id)
{
  auto fn = loader_->Resolve< OakNodeHandle(*)(const char*) >("oak_node_create");
  if (!fn) return nullptr;
  return fn(type_id);
}

void EngineAdapter::NodeDestroy(OakNodeHandle node)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle) >("oak_node_destroy");
  if (fn) fn(node);
}

const char *EngineAdapter::NodeTypeId(OakNodeHandle node)
{
  auto fn = loader_->Resolve< const char*(*)(OakNodeHandle) >("oak_node_type_id");
  if (!fn) return "";
  return fn(node);
}

const char *EngineAdapter::NodeLabel(OakNodeHandle node)
{
  auto fn = loader_->Resolve< const char*(*)(OakNodeHandle) >("oak_node_label");
  if (!fn) return "";
  return fn(node);
}

void EngineAdapter::NodeSetLabel(OakNodeHandle node, const char *label)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*) >("oak_node_set_label");
  if (fn) fn(node, label);
}

void EngineAdapter::NodeSetBool(OakNodeHandle node, const char *param_id, bool val)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, bool) >("oak_node_set_bool");
  if (fn) fn(node, param_id, val);
}

bool EngineAdapter::NodeGetBool(OakNodeHandle node, const char *param_id)
{
  auto fn = loader_->Resolve< bool(*)(OakNodeHandle, const char*) >("oak_node_get_bool");
  if (!fn) return false;
  return fn(node, param_id);
}

void EngineAdapter::NodeSetInt(OakNodeHandle node, const char *param_id, int val)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, int) >("oak_node_set_int");
  if (fn) fn(node, param_id, val);
}

int EngineAdapter::NodeGetInt(OakNodeHandle node, const char *param_id)
{
  auto fn = loader_->Resolve< int(*)(OakNodeHandle, const char*) >("oak_node_get_int");
  if (!fn) return 0;
  return fn(node, param_id);
}

void EngineAdapter::NodeSetInt64(OakNodeHandle node, const char *param_id, int64_t val)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, int64_t) >("oak_node_set_int64");
  if (fn) fn(node, param_id, val);
}

int64_t EngineAdapter::NodeGetInt64(OakNodeHandle node, const char *param_id)
{
  auto fn = loader_->Resolve< int64_t(*)(OakNodeHandle, const char*) >("oak_node_get_int64");
  if (!fn) return 0;
  return fn(node, param_id);
}

void EngineAdapter::NodeSetFloat(OakNodeHandle node, const char *param_id, float val)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, float) >("oak_node_set_float");
  if (fn) fn(node, param_id, val);
}

float EngineAdapter::NodeGetFloat(OakNodeHandle node, const char *param_id)
{
  auto fn = loader_->Resolve< float(*)(OakNodeHandle, const char*) >("oak_node_get_float");
  if (!fn) return 0.0f;
  return fn(node, param_id);
}

void EngineAdapter::NodeSetDouble(OakNodeHandle node, const char *param_id, double val)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, double) >("oak_node_set_double");
  if (fn) fn(node, param_id, val);
}

double EngineAdapter::NodeGetDouble(OakNodeHandle node, const char *param_id)
{
  auto fn = loader_->Resolve< double(*)(OakNodeHandle, const char*) >("oak_node_get_double");
  if (!fn) return 0.0;
  return fn(node, param_id);
}

void EngineAdapter::NodeSetString(OakNodeHandle node, const char *param_id, const char *val)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, const char*) >("oak_node_set_string");
  if (fn) fn(node, param_id, val);
}

const char *EngineAdapter::NodeGetString(OakNodeHandle node, const char *param_id)
{
  auto fn = loader_->Resolve< const char*(*)(OakNodeHandle, const char*) >("oak_node_get_string");
  if (!fn) return "";
  return fn(node, param_id);
}

void EngineAdapter::NodeSetRational(OakNodeHandle node, const char *param_id, int64_t num, int64_t den)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, int64_t, int64_t) >("oak_node_set_rational");
  if (fn) fn(node, param_id, num, den);
}

void EngineAdapter::NodeGetRational(OakNodeHandle node, const char *param_id, int64_t *out_num, int64_t *out_den)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, int64_t*, int64_t*) >("oak_node_get_rational");
  if (fn) fn(node, param_id, out_num, out_den);
}

void EngineAdapter::NodeSetVec2(OakNodeHandle node, const char *param_id, const float *v)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, const float*) >("oak_node_set_vec2");
  if (fn) fn(node, param_id, v);
}

void EngineAdapter::NodeGetVec2(OakNodeHandle node, const char *param_id, float *out_v)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, float*) >("oak_node_get_vec2");
  if (fn) fn(node, param_id, out_v);
}

void EngineAdapter::NodeSetVec3(OakNodeHandle node, const char *param_id, const float *v)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, const float*) >("oak_node_set_vec3");
  if (fn) fn(node, param_id, v);
}

void EngineAdapter::NodeGetVec3(OakNodeHandle node, const char *param_id, float *out_v)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, float*) >("oak_node_get_vec3");
  if (fn) fn(node, param_id, out_v);
}

void EngineAdapter::NodeSetVec4(OakNodeHandle node, const char *param_id, const float *v)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, const float*) >("oak_node_set_vec4");
  if (fn) fn(node, param_id, v);
}

void EngineAdapter::NodeGetVec4(OakNodeHandle node, const char *param_id, float *out_v)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, float*) >("oak_node_get_vec4");
  if (fn) fn(node, param_id, out_v);
}

void EngineAdapter::NodeSetColor(OakNodeHandle node, const char *param_id, const float *rgba)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, const float*) >("oak_node_set_color");
  if (fn) fn(node, param_id, rgba);
}

void EngineAdapter::NodeGetColor(OakNodeHandle node, const char *param_id, float *out_rgba)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*, float*) >("oak_node_get_color");
  if (fn) fn(node, param_id, out_rgba);
}

int EngineAdapter::NodeConnect(OakNodeHandle out_node, const char *output_id,
                                OakNodeHandle in_node, const char *input_id)
{
  auto fn = loader_->Resolve< int(*)(OakNodeHandle, const char*, OakNodeHandle, const char*) >("oak_node_connect");
  if (!fn) return -1;
  return fn(out_node, output_id, in_node, input_id);
}

void EngineAdapter::NodeDisconnect(OakNodeHandle node, const char *input_id)
{
  auto fn = loader_->Resolve< void(*)(OakNodeHandle, const char*) >("oak_node_disconnect");
  if (fn) fn(node, input_id);
}

// ================================================================
//  Node Factory
// ================================================================
bool EngineAdapter::NodeFactoryInit()
{
  if (!fn_node_factory_init_) return false;
  return fn_node_factory_init_() == 0;
}

void EngineAdapter::NodeFactoryShutdown()
{
  if (fn_node_factory_shutdown_) fn_node_factory_shutdown_();
}

// ================================================================
//  Color Manager
// ================================================================
bool EngineAdapter::ColorManagerSetupDefault()
{
  if (!fn_color_manager_setup_default_) return false;
  return fn_color_manager_setup_default_() == 0;
}

// ================================================================
//  Config
// ================================================================
bool EngineAdapter::ConfigLoad()
{
  if (!fn_config_load_) return false;
  return fn_config_load_() == 0;
}

void EngineAdapter::ConfigSave()
{
  if (fn_config_save_) fn_config_save_();
}

// ================================================================
//  Managers
// ================================================================
bool EngineAdapter::RenderManagerCreate()
{
  if (!fn_render_manager_create_) return false;
  return fn_render_manager_create_() == 0;
}

void EngineAdapter::RenderManagerDestroy()
{
  if (fn_render_manager_destroy_) fn_render_manager_destroy_();
}

void EngineAdapter::RenderManagerSetProject(OakEngineProjectHandle proj)
{
  if (fn_render_manager_set_project_) fn_render_manager_set_project_(proj);
}

bool EngineAdapter::FrameManagerCreate()
{
  if (!fn_frame_manager_create_) return false;
  return fn_frame_manager_create_() == 0;
}

void EngineAdapter::FrameManagerDestroy()
{
  if (fn_frame_manager_destroy_) fn_frame_manager_destroy_();
}

bool EngineAdapter::DiskManagerCreate()
{
  if (!fn_disk_manager_create_) return false;
  return fn_disk_manager_create_() == 0;
}

void EngineAdapter::DiskManagerDestroy()
{
  if (fn_disk_manager_destroy_) fn_disk_manager_destroy_();
}

bool EngineAdapter::ConformManagerCreate()
{
  if (!fn_conform_manager_create_) return false;
  return fn_conform_manager_create_() == 0;
}

void EngineAdapter::ConformManagerDestroy()
{
  if (fn_conform_manager_destroy_) fn_conform_manager_destroy_();
}

bool EngineAdapter::AudioManagerCreate()
{
  if (!fn_audio_manager_create_) return false;
  return fn_audio_manager_create_() == 0;
}

void EngineAdapter::AudioManagerDestroy()
{
  if (fn_audio_manager_destroy_) fn_audio_manager_destroy_();
}

// ================================================================
//  Serializer
// ================================================================
bool EngineAdapter::ProjectSerializerInit()
{
  if (!fn_project_serializer_init_) return false;
  return fn_project_serializer_init_() == 0;
}

void EngineAdapter::ProjectSerializerDestroy()
{
  if (fn_project_serializer_destroy_) fn_project_serializer_destroy_();
}

// ================================================================
//  Undo
// ================================================================
OakUndoStackHandle EngineAdapter::UndoStackCreate()
{
  if (!fn_undo_stack_create_) return nullptr;
  return fn_undo_stack_create_();
}

void EngineAdapter::UndoStackDestroy(OakUndoStackHandle stack)
{
  if (fn_undo_stack_destroy_) fn_undo_stack_destroy_(stack);
}

void EngineAdapter::UndoStackPush(OakUndoStackHandle stack, OakUndoCommandHandle cmd, const char *name)
{
  if (fn_undo_stack_push_) fn_undo_stack_push_(stack, cmd, name);
}

void EngineAdapter::UndoStackUndo(OakUndoStackHandle stack)
{
  if (fn_undo_stack_undo_) fn_undo_stack_undo_(stack);
}

void EngineAdapter::UndoStackRedo(OakUndoStackHandle stack)
{
  if (fn_undo_stack_redo_) fn_undo_stack_redo_(stack);
}

void EngineAdapter::UndoStackClear(OakUndoStackHandle stack)
{
  if (fn_undo_stack_clear_) fn_undo_stack_clear_(stack);
}

bool EngineAdapter::UndoStackCanUndo(OakUndoStackHandle stack)
{
  if (!fn_undo_stack_can_undo_) return false;
  return fn_undo_stack_can_undo_(stack);
}

bool EngineAdapter::UndoStackCanRedo(OakUndoStackHandle stack)
{
  if (!fn_undo_stack_can_redo_) return false;
  return fn_undo_stack_can_redo_(stack);
}

OakUndoCommandHandle EngineAdapter::UndoCommandMultiCreate()
{
  if (!fn_undo_command_multi_create_) return nullptr;
  return fn_undo_command_multi_create_();
}

void EngineAdapter::UndoCommandMultiAddChild(OakUndoCommandHandle multi, OakUndoCommandHandle child)
{
  if (fn_undo_command_multi_add_child_) fn_undo_command_multi_add_child_(multi, child);
}

void EngineAdapter::UndoCommandMultiDestroy(OakUndoCommandHandle multi)
{
  if (fn_undo_command_multi_destroy_) fn_undo_command_multi_destroy_(multi);
}

OakUndoCommandHandle EngineAdapter::CommandNodeAddCreate(OakEngineProjectHandle proj, OakNodeHandle node)
{
  if (!fn_command_node_add_create_) return nullptr;
  return fn_command_node_add_create_(proj, node);
}

OakUndoCommandHandle EngineAdapter::CommandFolderAddChildCreate(OakNodeHandle folder, OakNodeHandle child)
{
  if (!fn_command_folder_add_child_create_) return nullptr;
  return fn_command_folder_add_child_create_(folder, child);
}

OakUndoCommandHandle EngineAdapter::CommandNodeSetPositionCreate(OakNodeHandle node, const float *pos)
{
  if (!fn_command_node_set_position_create_) return nullptr;
  return fn_command_node_set_position_create_(node, pos);
}

OakUndoCommandHandle EngineAdapter::CommandNodeRenameCreate(OakNodeHandle node, const char *new_name)
{
  if (!fn_command_node_rename_create_) return nullptr;
  return fn_command_node_rename_create_(node, new_name);
}

OakUndoCommandHandle EngineAdapter::CustomUndoCommandCreate(void (*redo_fn)(void*), void (*undo_fn)(void*), void *userdata)
{
  if (!fn_undo_command_custom_create_) return nullptr;
  return fn_undo_command_custom_create_(redo_fn, undo_fn, userdata);
}

// ================================================================
//  Callbacks
// ================================================================
void EngineAdapter::ProjectSetModifiedCallback(OakEngineProjectHandle proj,
                                                void (*callback)(OakEngineProjectHandle, bool, void *),
                                                void *userdata)
{
  if (fn_project_set_modified_callback_) fn_project_set_modified_callback_(proj, callback, userdata);
}

// ================================================================
//  String cleanup
// ================================================================
void EngineAdapter::StringFree(const char *s)
{
  if (fn_string_free_) fn_string_free_(s);
}

} // namespace adapters
} // namespace olive
