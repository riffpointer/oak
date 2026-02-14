#ifndef APP_RENDER_CACHE_FRAMEDISKCACHE_H_
#define APP_RENDER_CACHE_FRAMEDISKCACHE_H_

#include "render/cache/framecache.h"
namespace olive::cache {

class FrameDiskCacheStore: public FrameCacheStore{
public:
	FrameCacheEntryPtr get(FrameCacheKey &key) override;
	void set(FrameCacheKey &key, FrameCacheEntryPtr entry) override;
	void remove(FrameCacheKey &key) override;
};
}

#endif  // APP_RENDER_CACHE_FRAMEDISKCACHE_H_