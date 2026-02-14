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
#include "framecache.h"
#include "framememcache.h"
#include <mutex>
#include <qsysinfo.h>
#include <spdlog/spdlog.h>
#include <xcb/xproto.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <QSysInfo>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <stdio.h>
#include <string.h>
#else // macOS
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#endif

uint64_t get_available_memory()
{
#ifdef _WIN32
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	if (GlobalMemoryStatusEx(&status)) {
		return status.ullAvailPhys; 
	}
	return 0;

#elif defined(__linux__)
	FILE *fp = fopen("/proc/meminfo", "r");
	if (!fp) {
		return 0;
	}

	char line[256];
	uint64_t available_kb = 0;

	while (fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "MemAvailable: %lu kB", &available_kb) == 1) {
			fclose(fp);
			return available_kb * 1024; 
		}
	}

	fclose(fp);
	return 0;

#else
	vm_size_t page_size;
	vm_statistics64_data_t vm_stats;
	mach_port_t mach_port = mach_host_self();
	mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);

    if (KERN_SUCCESS != host_page_size(mach_port, &page_size)) {
		return 0;
	}

	if (KERN_SUCCESS != host_statistics64(mach_port, HOST_VM_INFO64,
										  (host_info64_t)&vm_stats, &count)) {
		return 0;
	}

	return (uint64_t)vm_stats.free_count * page_size;
#endif
}

void my_sleep(unsigned int seconds)
{
#ifdef _WIN32
	Sleep(seconds * 1000);
#else
	sleep(seconds);
#endif
}

using namespace olive::cache;
FrameMemCacheStore FrameMemCacheStore::frame_cache_;

void FrameMemCacheStore::thread()
{
	while(!stop){
        std::unique_lock<std::mutex> lock(size_lock);
        lock.lock();
		cache_size = get_available_memory() * 0.25;

        size_t total_cache_size = 0;

        for(auto entry: map_){
            total_cache_size += entry->size();
        }

        if(total_cache_size > cache_size){
            while(total_cache_size > cache_size){
                FrameCacheKey key = lru_list.first();
                lru_list.removeFirst();
                size_t size=map_[key]->size();
                map_.remove(key);
                total_cache_size -= size;
            }
        }
        lock.unlock();
		my_sleep(5);
    }
}
FrameCacheEntryPtr FrameMemCacheStore::get(FrameCacheKey &key)
{
	if (map_.contains(key)) {
		if (lru_list.last() != key) {
			lru_list.removeOne(key);
			lru_list.append(key);
		}
		return map_[key];
	} else {
		return nullptr;
	}
}

void FrameMemCacheStore::set(FrameCacheKey &key, FrameCacheEntryPtr entry)
{
	std::lock_guard<std::mutex> lock(this->map_lock);
	map_[key] = entry;
	if (!lru_list.contains(key)) {
		lru_list.append(key);
	} else {
		lru_list.removeOne(key);
		lru_list.append(key);
	}
}

void FrameMemCacheStore::remove(FrameCacheKey &key)
{
	std::lock_guard<std::mutex> lock(this->map_lock);
	lru_list.removeOne(key);
	map_.remove(key);
}