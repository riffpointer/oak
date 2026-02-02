
#ifndef FRAME_CACHE_H
#define FRAME_CACHE_H
#include "codec/frame.h"
#include "olive/core/util/rational.h"
#include "render/cache/framehashcache.h"
#include "render/texture.h"
#include "render/videoparams.h"
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <QHash>
#include <QThread>
#include <qobject.h>
#include <qtmetamacros.h>
#include <thread>
#include <utility>
#include <hash>
#include <qhashfunctions.h>
namespace olive::cache{
class FrameCacheKey;
static size_t qHashForFrameCacheKey(const FrameCacheKey &key, size_t seed = 0);
class FrameCacheKey {
private:
    uint64_t version_ = 0;
    rational time_ = 0;
    VideoParams params_;
public:
    FrameCacheKey() = default;
    FrameCacheKey(uint64_t version, rational time, VideoParams &params)
        : version_(version), time_(time), params_(params){}
    uint64_t version() const{
        return version_;
    }
    rational time() const{
        return time_;
    }
    VideoParams params() const {
        return params_;
    }
    bool operator==(FrameCacheKey &other) const{
        return version_==other.version_ && time_ == other.time_
            && params_ == other.params_;
    }
    size_t qHash(const FrameCacheKey& key, size_t seed);
};
static size_t qHashForFrameCacheKey(const FrameCacheKey &key, size_t seed)
{
	const VideoParams params = key.params();
	return qHashMulti(seed,
					  key.version(),
					  key.time().toDouble(),
					  params.width(),
					  params.height(),
					  params.depth(),
					  params.time_base().numerator(),
					  params.time_base().denominator(),
					  static_cast<int>(params.format()),
					  params.channel_count(),
					  params.pixel_aspect_ratio().numerator(),
					  params.pixel_aspect_ratio().denominator(),
					  static_cast<int>(params.interlacing()),
					  params.divider(),
					  params.effective_width(),
					  params.effective_height(),
					  params.effective_depth(),
					  params.square_pixel_width(),
					  params.enabled(),
					  params.stream_index(),
					  static_cast<int>(params.video_type()),
					  params.frame_rate().numerator(),
					  params.frame_rate().denominator(),
					  params.start_time(),
					  params.duration(),
					  params.premultiplied_alpha(),
					  params.colorspace(),
					  params.x(),
					  params.y(),
					  static_cast<int>(params.color_range()));
}
class FrameCacheEntry{
private:
    TexturePtr texture_;
    FramePtr frame_;
    time_t last_use;
public:
    FrameCacheEntry(){
        last_use = std::time(nullptr);
    };
    explicit FrameCacheEntry(TexturePtr &texture){
        texture_ = texture;
		last_use = std::time(nullptr);
	}
	explicit FrameCacheEntry(FramePtr &frame)
	{
		frame = frame;
		last_use = std::time(nullptr);
	}
	TexturePtr texture(){
        last_use = std::time(nullptr);
        return texture_;
    }

    FramePtr frame(){
        return frame_;
    }

    bool is_cpu(){
        return texture_ == nullptr;
    }

    time_t last_use_time(){
        return last_use;
    }

    size_t size();

};
using FrameCacheEntryPtr = std::shared_ptr<FrameCacheEntry>;
class FrameCacheStore {
public:

	virtual FrameCacheEntryPtr get(FrameCacheKey &key) = 0;
	virtual void set(FrameCacheKey &key, FrameCacheEntryPtr entry) = 0;
	virtual void remove(FrameCacheKey &key) = 0;
protected:
	void thread();
};
}
#endif
