#include "oak_engine_runtime.h"
#include <QDebug>

#ifdef __APPLE__
#include <dlfcn.h>
#define LIBNAME "liboakengine.dylib"
#elif defined(__linux__)
#include <dlfcn.h>
#define LIBNAME "liboakengine.so"
#elif defined(_WIN32)
#include <windows.h>
#define LIBNAME "liboakengine.dll"
#endif

OakEngineRuntime* OakEngineRuntime::Instance()
{
    static OakEngineRuntime instance;
    return &instance;
}

bool OakEngineRuntime::Load()
{
    if (IsLoaded()) return true;

#ifdef _WIN32
    handle_ = LoadLibraryA(LIBNAME);
#else
    handle_ = dlopen(LIBNAME, RTLD_LAZY | RTLD_GLOBAL);
#endif

    if (!handle_) {
        qWarning() << "Failed to load" << LIBNAME;
        return false;
    }

    project_load_xml = reinterpret_cast<decltype(project_load_xml)>(GetSymbol("oak_engine_project_load_xml"));
    project_destroy = reinterpret_cast<decltype(project_destroy)>(GetSymbol("oak_engine_project_destroy"));
    project_node_count = reinterpret_cast<decltype(project_node_count)>(GetSymbol("oak_engine_project_node_count"));
    session_create = reinterpret_cast<decltype(session_create)>(GetSymbol("oak_engine_session_create"));
    session_destroy = reinterpret_cast<decltype(session_destroy)>(GetSymbol("oak_engine_session_destroy"));
    session_render_frame = reinterpret_cast<decltype(session_render_frame)>(GetSymbol("oak_engine_session_render_frame"));

    if (!project_load_xml || !session_create || !session_render_frame) {
        qWarning() << "Failed to resolve essential engine symbols";
        return false;
    }

    return true;
}

void* OakEngineRuntime::GetSymbol(const char* name)
{
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
    return dlsym(handle_, name);
#endif
}
