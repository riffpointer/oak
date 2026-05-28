#ifndef OAK_DLOPEN_HELPER_H
#define OAK_DLOPEN_HELPER_H

#include <QString>
#include <memory>

namespace olive {
namespace adapters {

/**
 * @brief Cross-platform dynamic library loader
 *
 * Wraps dlopen/dlsym/dlclose (POSIX) and LoadLibrary/GetProcAddress/FreeLibrary (Windows).
 */
class DylibLoader {
public:
  DylibLoader();
  explicit DylibLoader(const QString &path);
  ~DylibLoader();

  bool Load(const QString &path);
  bool IsLoaded() const;
  void *Resolve(const char *symbol) const;
  void Unload();

  template<typename T>
  T Resolve(const char *symbol) const {
    return reinterpret_cast<T>(Resolve(symbol));
  }

  QString LastError() const;

private:
  void *handle_ = nullptr;
  mutable QString last_error_;
};

} // namespace adapters
} // namespace olive

#endif // OAK_DLOPEN_HELPER_H
