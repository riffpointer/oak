/***

  Engine-private AVFrame wrapper (transitional)
  This header is intentionally NOT in OakShared so that OakShared remains
  FFmpeg-free.  It lives in engine/ and will be removed once PluginRenderer
  and OliveClip stop using AVFrame directly.

***/

#ifndef ENGINE_AVFRAME_TYPES_H
#define ENGINE_AVFRAME_TYPES_H

extern "C" {
#include <libavutil/frame.h>
}

#include <memory>

namespace olive {

using AVFramePtr = std::shared_ptr<AVFrame>;

inline AVFramePtr CreateAVFramePtr(AVFrame *f)
{
    return std::shared_ptr<AVFrame>(f, [](AVFrame *g) { av_frame_free(&g); });
}

inline AVFramePtr CreateAVFramePtr()
{
    return CreateAVFramePtr(av_frame_alloc());
}

inline AVFramePtr CloneAVFramePtr(void* internal_avframe)
{
    AVFrame* copied = av_frame_alloc();
    av_frame_ref(copied, static_cast<AVFrame*>(internal_avframe));
    return CreateAVFramePtr(copied);
}

}

#endif // ENGINE_AVFRAME_TYPES_H
