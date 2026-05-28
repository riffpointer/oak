/*
 *  oakengine.so C API — Manager Singleton Lifecycle
 *  RenderManager, FrameManager, DiskManager, ConformManager, AudioManager
 */

#include "oak/engine_api.h"

#include "render/rendermanager.h"
#include "render/diskmanager.h"
#include "audio/audiomanager.h"
#include "node/factory.h"
#include "node/project/serializer/serializer.h"

// FrameManager is in shared (oakshared static), but app may need it via engine
#include "olive/render/framemanager.h"

// ConformManager is in oakcodec, engine will route to it internally
// For now we stub it; later engine can dlopen oakcodec
namespace olive {
class ConformManagerStub {
public:
  static void CreateInstance() {}
  static void DestroyInstance() {}
};
}

extern "C" {

/* ------------------------------------------------------------------ */
/*  RenderManager                                                      */
/* ------------------------------------------------------------------ */
int oak_render_manager_create(void)
{
  olive::RenderManager::CreateInstance();
  return 0;
}

void oak_render_manager_destroy(void)
{
  olive::RenderManager::DestroyInstance();
}

void oak_render_manager_set_project(OakEngineProjectHandle proj)
{
  if (!proj) return;
  olive::RenderManager::instance()->SetProject(reinterpret_cast<olive::Project*>(proj));
}

/* ------------------------------------------------------------------ */
/*  FrameManager                                                       */
/* ------------------------------------------------------------------ */
int oak_frame_manager_create(void)
{
  olive::FrameManager::CreateInstance();
  return 0;
}

void oak_frame_manager_destroy(void)
{
  olive::FrameManager::DestroyInstance();
}

/* ------------------------------------------------------------------ */
/*  DiskManager                                                        */
/* ------------------------------------------------------------------ */
int oak_disk_manager_create(void)
{
  olive::DiskManager::CreateInstance();
  return 0;
}

void oak_disk_manager_destroy(void)
{
  olive::DiskManager::DestroyInstance();
}

/* ------------------------------------------------------------------ */
/*  ConformManager                                                     */
/* ------------------------------------------------------------------ */
int oak_conform_manager_create(void)
{
  // FIXME: ConformManager is in oakcodec.so. Engine should dlopen oakcodec
  // and call its C API. For now we stub.
  olive::ConformManagerStub::CreateInstance();
  return 0;
}

void oak_conform_manager_destroy(void)
{
  olive::ConformManagerStub::DestroyInstance();
}

/* ------------------------------------------------------------------ */
/*  AudioManager                                                       */
/* ------------------------------------------------------------------ */
int oak_audio_manager_create(void)
{
  olive::AudioManager::CreateInstance();
  return 0;
}

void oak_audio_manager_destroy(void)
{
  olive::AudioManager::DestroyInstance();
}

/* ------------------------------------------------------------------ */
/*  NodeFactory                                                        */
/* ------------------------------------------------------------------ */
int oak_node_factory_initialize(void)
{
  olive::NodeFactory::Initialize();
  return 0;
}

void oak_node_factory_shutdown(void)
{
  olive::NodeFactory::Destroy();
}

/* ------------------------------------------------------------------ */
/*  ColorManager                                                       */
/* ------------------------------------------------------------------ */
int oak_color_manager_setup_default(void)
{
  olive::ColorManager::SetUpDefaultConfig();
  return 0;
}

/* ------------------------------------------------------------------ */
/*  Config                                                             */
/* ------------------------------------------------------------------ */
int oak_config_load(void)
{
  olive::Config::Load();
  return 0;
}

void oak_config_save(void)
{
  olive::Config::Save();
}

/* ------------------------------------------------------------------ */
/*  ProjectSerializer                                                  */
/* ------------------------------------------------------------------ */
int oak_project_serializer_initialize(void)
{
  olive::ProjectSerializer::Initialize();
  return 0;
}

void oak_project_serializer_destroy(void)
{
  olive::ProjectSerializer::Destroy();
}

} // extern "C"
