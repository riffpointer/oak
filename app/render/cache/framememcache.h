#ifndef APP_RENDER_CACHE_FRAMEMEMCACHE_H_
#define APP_RENDER_CACHE_FRAMEMEMCACHE_H_
#include "render/cache/framecache.h"
namespace olive::cache {
/*
    * LRU Frame Cache
    * TODO: This is just a mininum version. It should 
    * be replaced by a more complex one.
    * We use 25% of availiable memory as LRU Cache.
    */
class FrameMemCacheStore : FrameCacheStore {
private:
	QHash<FrameCacheKey, FrameCacheEntryPtr> map_;
	static FrameMemCacheStore frame_cache_;
	std::mutex map_lock;
	std::mutex size_lock;
	size_t cache_size;
	bool stop = false;
	std::thread *gc_thread;
	QList<FrameCacheKey> lru_list;

public:
	static FrameMemCacheStore *getInstance()
	{
		return &frame_cache_;
	}
	FrameMemCacheStore()
	{
		auto f = std::bind(&FrameMemCacheStore::thread, this);
		gc_thread = new std::thread(f);
	};
	~FrameMemCacheStore()
	{
		finalize();
	};

	FrameCacheEntryPtr get(FrameCacheKey &key) override;
	void set(FrameCacheKey &key, FrameCacheEntryPtr entry) override;
	void remove(FrameCacheKey &key) override;

	void finalize()
	{
		stop = true;
		gc_thread->join();
		delete gc_thread;
	}

protected:
	void thread();
};
}
#endif  // APP_RENDER_CACHE_FRAMEMEMCACHE_H_