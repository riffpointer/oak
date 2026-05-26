#ifndef OAK_ENGINE_RUNTIME_H
#define OAK_ENGINE_RUNTIME_H

#include <QString>

class OakEngineRuntime {
public:
    static OakEngineRuntime* Instance();

    bool Load();
    bool IsLoaded() const { return handle_ != nullptr; }

    void* (*project_load_xml)(const char* xml_str) = nullptr;
    void (*project_destroy)(void* proj) = nullptr;
    int (*project_node_count)(void* proj) = nullptr;

    void* (*session_create)(void* proj, int width, int height,
                            int pixel_format,
                            int64_t timebase_num, int64_t timebase_den) = nullptr;
    void (*session_destroy)(void* session) = nullptr;
    int (*session_render_frame)(void* session, int64_t time_num, int64_t time_den,
                                void* out_frame) = nullptr;

private:
    OakEngineRuntime() = default;
    void* handle_ = nullptr;

    void* GetSymbol(const char* name);
};

#endif // OAK_ENGINE_RUNTIME_H
