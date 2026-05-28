/*
 *  oakengine.so C API — Callbacks
 *  Bridge Qt signals to C callbacks for cross-so event delivery.
 */

#include "oak/engine_api.h"

#include <QObject>
#include "node/project.h"

extern "C" {

void oak_project_set_modified_callback(OakEngineProjectHandle proj,
                                       OakProjectModifiedCallback cb,
                                       void* userdata)
{
  if (!proj || !cb) return;

  auto* p = reinterpret_cast<olive::Project*>(proj);

  // Connect using a lambda that captures the C callback
  QObject::connect(p, &olive::Project::ModifiedChanged,
                   [cb, userdata, proj](bool modified) {
                     cb(proj, modified, userdata);
                   });
}

} // extern "C"
