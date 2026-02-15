/*
 * Oak Video Editor - Non-Linear Video Editor
 * Copyright (C) 2025 Olive CE Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "codec/frame.h"
#include "olive/core/util/rational.h"
#include "render/videoparams.h"
#include <atomic>
#include <cstdint>
#include <ctime>
#include <locale>
#include <qhash.h>
#include <quuid.h>
#include <thread>
#include "render/playbackcache.h"
#include <QHash>
#include <QMutex>
namespace olive{
class FrameMemCacheKey{
public:
    static FrameMemCacheKey create(QUuid uuid, int64_t time, const VideoParams &params){
        FrameMemCacheKey key;
        key.uuid_=uuid;
        key.params=params;
        key.time=time;
        key.timestamp_=std::time(nullptr);
        return key;
    }
    friend uint qHash(const FrameMemCacheKey &key, uint seed);
    bool operator==(const FrameMemCacheKey &that) const{
        if(this->uuid_ == that.uuid_ 
            && this->time == that.time 
            && this->params == that.params
            && this->timestamp_ == that.timestamp_
        ){
            return true;
        }
        return false;
    }
    std::time_t timestamp() const{
        return timestamp_;
    }
    QUuid uuid() const{
        return uuid_;
    }
    int64_t frame_time() const{
        return time;
    }
private:
	FrameMemCacheKey() = default;
    ~FrameMemCacheKey() = default;
	QUuid uuid_;
    std::time_t timestamp_ = 0;
    VideoParams params;
	int64_t time = 0;
};

inline uint qHash(const FrameMemCacheKey &key, uint seed = 0)
{
    return qHashMulti(seed,
                      key.uuid_,
                      static_cast<quint64>(key.timestamp_),
                      key.time,
                      key.params.width(),
                      key.params.height(),
                      key.params.depth(),
                      key.params.divider(),
                      key.params.channel_count(),
                      static_cast<int>(key.params.format()),
                      static_cast<int>(key.params.interlacing()),
                      key.params.time_base().toDouble(),
                      key.params.pixel_aspect_ratio().toDouble());
}

class FrameMemCacheValue{
public:
    static FrameMemCacheValue create(FramePtr frame){
        FrameMemCacheValue value;
        value.frame_ = frame;
        value.size_bytes_ = frame->allocated_size()*1.3;
        return value;
    }
    FramePtr frame() const{
        return frame_;
    }

    size_t size_bytes() const{
        return size_bytes_;
    }
private:
	FrameMemCacheValue() = default;
	FramePtr frame_;
    size_t size_bytes_ = 0;
};

class FrameMemCache : public PlaybackCache {
public:
    FrameMemCache();
    ~FrameMemCache();

	bool SaveCacheFrame(const int64_t &time, FramePtr frame) override;

    FramePtr LoadCacheFrame(const int64_t &time) const override;
    void InvalidateAll();
private: 
    static void StartLruThread();
    static void StopLruThread();
    static void LruWorkerLoop();
    static QHash<FrameMemCacheKey, FrameMemCacheValue> cache_;
    static QMutex cache_mutex_;
    static std::thread lru_thread_;
    static std::atomic<bool> lru_thread_running_;
    static std::atomic<bool> lru_thread_stop_requested_;
    static std::atomic<int> instance_count_;
    static std::atomic<int64_t> budget;
    static void doLru();
};
}
