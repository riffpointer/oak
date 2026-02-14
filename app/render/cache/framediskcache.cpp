#include "framediskcache.h"
#include "render/cache/framecache.h"
#include "render/cache/framehashcache.h"
#include <memory>
using namespace olive::cache;

FrameCacheEntryPtr FrameDiskCacheStore::get(FrameCacheKey& key){
    FramePtr frame=FrameHashCache::LoadCacheFrame(key.path(), key.uuid(), key.time().toDouble());
    return std::make_shared<FrameCacheEntry>(frame);
}

void FrameDiskCacheStore::set(FrameCacheKey& key, FrameCacheEntryPtr entry){
    
}