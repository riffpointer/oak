/*
 *  oakengine.so C API — Engine Lifecycle
 *  Subsystem initialization and shutdown.
 */

#include "oak/engine_api.h"

#include "node/factory.h"
#include "node/color/colormanager/colormanager.h"
#include "node/project/serializer/serializer.h"

extern "C" {

int oak_engine_init_subsystems(void)
{
  olive::NodeFactory::Initialize();
  olive::ColorManager::SetUpDefaultConfig();
  olive::ProjectSerializer::Initialize();
  return 0;
}

void oak_engine_shutdown_subsystems(void)
{
  olive::ProjectSerializer::Destroy();
  olive::NodeFactory::Destroy();
}

} // extern "C"
