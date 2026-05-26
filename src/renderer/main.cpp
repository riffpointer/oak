#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cstdio>
#include <iostream>

#include "runtime/oak_engine_runtime.h"
#include "oak/frame_api.h"

struct RendererState {
    void* project = nullptr;
    void* session = nullptr;
};

static void WriteJson(const QJsonObject& obj)
{
    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    std::cout << data.constData() << std::endl;
}

static void HandleLoadGraph(RendererState& state, const QJsonObject& req)
{
    QString xml = req.value("graph_xml").toString();
    if (xml.isEmpty()) {
        // Try graph_json as fallback (but we only support XML for now)
        xml = req.value("graph_json").toString();
    }

    auto rt = OakEngineRuntime::Instance();
    if (!rt->IsLoaded() && !rt->Load()) {
        QJsonObject resp;
        resp["seq"] = req.value("seq");
        resp["status"] = "error";
        resp["error"] = "Failed to load oakengine.so";
        WriteJson(resp);
        return;
    }

    if (state.project) {
        rt->project_destroy(state.project);
        state.project = nullptr;
    }
    if (state.session) {
        rt->session_destroy(state.session);
        state.session = nullptr;
    }

    state.project = rt->project_load_xml(xml.toUtf8().constData());
    if (!state.project) {
        QJsonObject resp;
        resp["seq"] = req.value("seq");
        resp["status"] = "error";
        resp["error"] = "Failed to load project";
        WriteJson(resp);
        return;
    }

    int node_count = rt->project_node_count(state.project);

    QJsonObject resp;
    resp["seq"] = req.value("seq");
    resp["status"] = "ok";
    resp["node_count"] = node_count;
    WriteJson(resp);
}

static void HandleConfig(RendererState& state, const QJsonObject& req)
{
    auto rt = OakEngineRuntime::Instance();
    if (!state.project) {
        QJsonObject resp;
        resp["seq"] = req.value("seq");
        resp["status"] = "error";
        resp["error"] = "No project loaded";
        WriteJson(resp);
        return;
    }

    if (state.session) {
        rt->session_destroy(state.session);
        state.session = nullptr;
    }

    int width = req.value("width").toInt(1920);
    int height = req.value("height").toInt(1080);
    int64_t timebase_num = req.value("timebase_num").toInt(24);
    int64_t timebase_den = req.value("timebase_den").toInt(1);

    // PixelFormat::F32 = 8 (internal enum)
    state.session = rt->session_create(state.project, width, height, 8,
                                       timebase_num, timebase_den);

    QJsonObject resp;
    resp["seq"] = req.value("seq");
    resp["status"] = state.session ? "ok" : "error";
    if (!state.session) {
        resp["error"] = "Failed to create session";
    }
    WriteJson(resp);
}

static void HandleRenderFrame(RendererState& state, const QJsonObject& req)
{
    auto rt = OakEngineRuntime::Instance();
    if (!state.session) {
        QJsonObject resp;
        resp["seq"] = req.value("seq");
        resp["status"] = "error";
        resp["error"] = "No session configured";
        WriteJson(resp);
        return;
    }

    int64_t frame_number = req.value("frame_number").toInt(0);
    // For simplicity, assume timebase is 24/1, so time = frame_number / 24
    int64_t time_num = frame_number;
    int64_t time_den = 24;

    OakFrame frame{};
    int ret = rt->session_render_frame(state.session, time_num, time_den, &frame);

    if (ret != 0) {
        QJsonObject resp;
        resp["seq"] = req.value("seq");
        resp["status"] = "error";
        resp["error"] = "Render failed";
        WriteJson(resp);
        return;
    }

    QJsonObject resp;
    resp["seq"] = req.value("seq");
    resp["status"] = "ok";
    resp["frame_number"] = static_cast<int>(frame_number);
    resp["width"] = frame.width;
    resp["height"] = frame.height;
    resp["pix_fmt"] = "rgba32f";
    resp["colorspace"] = "ACES - ACEScg";
    resp["stride"] = frame.stride[0];
    WriteJson(resp);
}

static void HandleExit(RendererState& state, const QJsonObject& req)
{
    auto rt = OakEngineRuntime::Instance();
    if (state.session) {
        rt->session_destroy(state.session);
        state.session = nullptr;
    }
    if (state.project) {
        rt->project_destroy(state.project);
        state.project = nullptr;
    }

    QJsonObject resp;
    resp["seq"] = req.value("seq");
    resp["status"] = "ok";
    WriteJson(resp);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    RendererState state;

    QTextStream in(stdin);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.isEmpty()) continue;

        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) continue;

        QJsonObject req = doc.object();
        QString cmd = req.value("cmd").toString();

        if (cmd == "load_graph") {
            HandleLoadGraph(state, req);
        } else if (cmd == "config") {
            HandleConfig(state, req);
        } else if (cmd == "render_frame") {
            HandleRenderFrame(state, req);
        } else if (cmd == "exit") {
            HandleExit(state, req);
            break;
        } else {
            QJsonObject resp;
            resp["seq"] = req.value("seq");
            resp["status"] = "error";
            resp["error"] = "Unknown command: " + cmd;
            WriteJson(resp);
        }
    }

    HandleExit(state, QJsonObject());
    return 0;
}
