/***

  Oak Video Editor - Non-Linear Video Editor
  Copyright (C) 2026 Oak Team

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

***/
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