/*
 *  oakengine.so C API Implementation
 *  Wraps existing olive::Project, olive::RenderProcessor, etc.
 */

#include "oak/engine_api.h"

#include <QXmlStreamReader>
#include <QVariant>

#include "node/project.h"
#include "node/serializeddata.h"
#include "node/output/viewer/viewer.h"
#include "render/renderprocessor.h"
#include "render/rendercache.h"
#include "render/rendermanager.h"
#include "render/opengl/opengl_renderer_proxy.h"
#include "olive/render/frame.h"

namespace olive {

class EngineRenderSession {
public:
	Project* project = nullptr;
	OpenGLRendererProxy* renderer = nullptr;
	DecoderCache decoder_cache;
	ShaderCache shader_cache;
	VideoParams video_params;
	FramePtr last_frame;
};

static ViewerOutput* FindViewerOutput(Project* project)
{
	for (Node* node : project->nodes()) {
		if (ViewerOutput* viewer = dynamic_cast<ViewerOutput*>(node)) {
			return viewer;
		}
	}
	return nullptr;
}

} // namespace olive

extern "C" {

OakEngineProjectHandle oak_engine_project_load_xml(const char* xml_str)
{
	if (!xml_str) return nullptr;
	auto* proj = new olive::Project();
	QXmlStreamReader reader(xml_str);
	proj->Load(&reader);
	return reinterpret_cast<OakEngineProjectHandle>(proj);
}

void oak_engine_project_destroy(OakEngineProjectHandle proj)
{
	if (!proj) return;
	auto* p = reinterpret_cast<olive::Project*>(proj);
	p->Clear();
	delete p;
}

int oak_engine_project_node_count(OakEngineProjectHandle proj)
{
	if (!proj) return 0;
	auto* p = reinterpret_cast<olive::Project*>(proj);
	return p->nodes().size();
}

OakEngineSessionHandle oak_engine_session_create(OakEngineProjectHandle proj,
                                                 int width, int height,
                                                 int pixel_format,
                                                 int64_t timebase_num, int64_t timebase_den)
{
	if (!proj) return nullptr;
	auto* project = reinterpret_cast<olive::Project*>(proj);

	auto* session = new olive::EngineRenderSession();
	session->project = project;

	// Initialize renderer
	session->renderer = new olive::OpenGLRendererProxy();
	if (!session->renderer->Init()) {
		delete session->renderer;
		delete session;
		return nullptr;
	}
	session->renderer->PostInit();

	// Setup video params
	session->video_params = olive::VideoParams(width, height,
										   static_cast<olive::PixelFormat::Format>(pixel_format),
										   olive::VideoParams::kRGBAChannelCount);
	session->video_params.set_time_base(olive::rational(timebase_num, timebase_den));

	return reinterpret_cast<OakEngineSessionHandle>(session);
}

void oak_engine_session_destroy(OakEngineSessionHandle session)
{
	if (!session) return;
	auto* s = reinterpret_cast<olive::EngineRenderSession*>(session);
	if (s->renderer) {
		s->renderer->PostDestroy();
		delete s->renderer;
	}
	delete s;
}

int oak_engine_session_render_frame(OakEngineSessionHandle session,
                                    int64_t time_num, int64_t time_den,
                                    OakFrame* out_frame)
{
	if (!session || !out_frame) return -1;
	auto* s = reinterpret_cast<olive::EngineRenderSession*>(session);

	olive::ViewerOutput* viewer = olive::FindViewerOutput(s->project);
	if (!viewer) {
		return -1;
	}

	// Create ticket
	olive::RenderTicketPtr ticket = std::make_shared<olive::RenderTicket>();
	ticket->setProperty("node", olive::QtUtils::PtrToValue(viewer));
	ticket->setProperty("time", QVariant::fromValue(olive::rational(time_num, time_den)));
	ticket->setProperty("type", olive::RenderManager::kTypeVideo);
	ticket->setProperty("return", olive::RenderManager::kFrame);
	ticket->setProperty("colormanager",
						olive::QtUtils::PtrToValue(s->project->color_manager()));
	ticket->setProperty("vparam", QVariant::fromValue(s->video_params));

	// Run render synchronously
	olive::RenderProcessor::Process(ticket, s->renderer, &s->decoder_cache, &s->shader_cache);

	// Wait for completion
	ticket->WaitForFinished();

	if (!ticket->HasResult()) {
		return -1;
	}

	olive::FramePtr frame = ticket->Get().value<olive::FramePtr>();
	if (!frame || !frame->is_allocated()) {
		return -1;
	}

	s->last_frame = frame;

	// Fill OakFrame
	out_frame->width = frame->width();
	out_frame->height = frame->height();
	out_frame->pix_fmt = OAK_FRAME_PIX_RGBA32F; // Engine internal is F32
	out_frame->storage = OAK_FRAME_CPU;
	out_frame->planes = 1;
	out_frame->data[0] = frame->data();
	out_frame->stride[0] = frame->linesize_bytes();
	out_frame->pts_num = time_num;
	out_frame->pts_den = time_den;
	out_frame->internal = nullptr;

	return 0;
}

} // extern "C"
