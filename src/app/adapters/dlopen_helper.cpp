#include "dlopen_helper.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace olive {
namespace adapters {

DylibLoader::DylibLoader()
{
}

DylibLoader::DylibLoader(const QString &path)
{
  Load(path);
}

DylibLoader::~DylibLoader()
{
  Unload();
}

bool DylibLoader::Load(const QString &path)
{
  Unload();

#ifdef _WIN32
  handle_ = LoadLibraryW(reinterpret_cast<const wchar_t *>(path.utf16()));
  if (!handle_) {
    last_error_ = QStringLiteral("LoadLibrary failed: %1").arg(GetLastError());
    return false;
  }
#else
  handle_ = dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);
  if (!handle_) {
    last_error_ = QString::fromUtf8(dlerror());
    return false;
  }
#endif

  return true;
}

bool DylibLoader::IsLoaded() const
{
  return handle_ != nullptr;
}

void *DylibLoader::Resolve(const char *symbol) const
{
  if (!handle_) return nullptr;

#ifdef _WIN32
  void *addr = reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(handle_), symbol));
  if (!addr) {
    last_error_ = QStringLiteral("GetProcAddress failed: %1").arg(GetLastError());
  }
  return addr;
#else
  dlerror(); // clear previous error
  void *addr = dlsym(handle_, symbol);
  const char *err = dlerror();
  if (err) {
    last_error_ = QString::fromUtf8(err);
    return nullptr;
  }
  return addr;
#endif
}

void DylibLoader::Unload()
{
  if (!handle_) return;

#ifdef _WIN32
  FreeLibrary(static_cast<HMODULE>(handle_));
#else
  dlclose(handle_);
#endif

  handle_ = nullptr;
}

QString DylibLoader::LastError() const
{
  return last_error_;
}

} // namespace adapters
} // namespace olive
