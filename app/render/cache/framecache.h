
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

    size_t size(){
        int byte_per_channel = texture_->format().byte_count();
        int width = texture_->width();
        int height = texture_->height();
        int channel_count = texture_->channel_count();
		int overhead_factor = 1.3; // std::shared_ptr, metadata
        return byte_per_channel * width * height * channel_count * overhead_factor;
	}

};
using FrameCacheEntryPtr = std::shared_ptr<FrameCacheEntry>;
/*
 * LRU Frame Cache
 * TODO: This is just a mininum version. It should 
 * be replaced by a more complex one.
 * We use 25% of availiable memory as LRU Cache.
 */
class FrameMemCache {
private:
    QHash<FrameCacheKey, FrameCacheEntryPtr> map_;
    static FrameMemCache frame_cache_;
    std::mutex map_lock;
    std::mutex size_lock;
    size_t cache_size;
    bool stop = false;
    std::thread *gc_thread;
    QList<FrameCacheKey> lru_list;
public:
	static FrameMemCache *getInstance()
	{
		return &frame_cache_;
	}
	FrameMemCache()
	{
		auto f = std::bind(&FrameMemCache::thread, this);
		gc_thread=new std::thread(f);
	};
	~FrameMemCache()
	{
		finalize();
	};

	FrameCacheEntryPtr get(FrameCacheKey& key);
    void set(FrameCacheKey &key, FrameCacheEntryPtr entry);
    void remove(FrameCacheKey &key);

    void finalize(){
        stop = true;
        gc_thread->join();
        delete gc_thread;
    }
protected:
    void thread();
};
}
#endif
