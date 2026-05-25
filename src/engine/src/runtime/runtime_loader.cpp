/***  Oak Video Editor - Runtime Module Loader  Copyright (C) 2025 mikesolar  ***/

#include "runtime_loader.h"

#if defined(__APPLE__)
#include <dlfcn.h>
#elif defined(__linux__)
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <QDebug>

namespace olive {

OakRuntimeLoader::~OakRuntimeLoader()
{
    if (handle_) {
#if defined(__APPLE__) || defined(__linux__)
        dlclose(handle_);
#elif defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(handle_));
#endif
    }
}

bool OakRuntimeLoader::Load(const QString& lib_name)
{
    if (handle_) {
        return true;
    }

#if defined(__APPLE__)
    handle_ = dlopen(lib_name.toUtf8().constData(), RTLD_NOW | RTLD_GLOBAL);
#elif defined(__linux__)
    handle_ = dlopen(lib_name.toUtf8().constData(), RTLD_NOW | RTLD_GLOBAL);
#elif defined(_WIN32)
    handle_ = static_cast<void*>(LoadLibraryW(lib_name.toStdWString().c_str()));
#endif

    if (!handle_) {
        qWarning() << "Failed to load" << lib_name << ":"
#if defined(__APPLE__) || defined(__linux__)
                   << dlerror()
#endif
            ;
        return false;
    }

    return true;
}

void* OakRuntimeLoader::GetSymbolRaw(const char* name)
{
    if (!handle_) {
        return nullptr;
    }

#if defined(__APPLE__) || defined(__linux__)
    void* sym = dlsym(handle_, name);
    if (!sym) {
        qWarning() << "Failed to resolve symbol" << name << ":" << dlerror();
    }
    return sym;
#elif defined(_WIN32)
    return static_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
    return nullptr;
#endif
}

} // namespace olive
