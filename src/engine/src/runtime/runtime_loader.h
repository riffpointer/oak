/***  Oak Video Editor - Runtime Module Loader  Copyright (C) 2025 mikesolar  ***/

#ifndef RUNTIME_LOADER_H
#define RUNTIME_LOADER_H

#include <QString>

namespace olive {

class OakRuntimeLoader {
public:
    OakRuntimeLoader() = default;
    virtual ~OakRuntimeLoader();

    bool Load(const QString& lib_name);
    bool IsLoaded() const { return handle_ != nullptr; }

    template <typename T>
    T GetSymbol(const char* name)
    {
        return reinterpret_cast<T>(GetSymbolRaw(name));
    }

protected:
    void* GetSymbolRaw(const char* name);

    void* handle_ = nullptr;
};

} // namespace olive

#endif // RUNTIME_LOADER_H
