
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
#include <quuid.h>
#include <thread>
#include <utility>
#include <hash>
#include <qhashfunctions.h>
namespace olive::cache{
class FrameCacheKey;
size_t qHash(const FrameCacheKey &key, size_t seed = 0);
class FrameCacheKey {
private:
    uint64_t version_ = 0;
    rational time_ = 0;
    VideoParams params_;
	QString path_;
	QUuid uuid_;
public:
	FrameCacheKey() = default;
    FrameCacheKey(uint64_t version, rational time, VideoParams &params)
        : version_(version), time_(time), params_(params){
            uuid_=QUuid::createUuid();
        }
	FrameCacheKey(uint64_t version, rational time, VideoParams &params,
				  QString &path, QUuid uuid)
		: version_(version)
		, time_(time)
		, params_(params)
		, path_(path)
		, uuid_(uuid)
	{
	}
	[[nodiscard]] uint64_t version() const{
        return version_;
    }
    [[nodiscard]] rational time() const{
        return time_;
    }
    [[nodiscard]] VideoParams params() const {
        return params_;
    }
    void set_path(QString &path){
        this->path_=path;
    }
	void set_uuid(QUuid &uuid)
	{
		this->uuid_ = uuid;
	}
	[[nodiscard]] QString path() const{
        return path_;
    }
	[[nodiscard]] QUuid uuid() const
	{
		return uuid_;
	}
	bool operator==(FrameCacheKey &other) const{
        return version_==other.version_ && time_ == other.time_
            && params_ == other.params_ && path_ == other.path_;
    }
    size_t qHash(const FrameCacheKey& key, size_t seed);
};
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
		frame_ = frame;
		last_use = std::time(nullptr);
	}
	TexturePtr texture(){
        last_use = std::time(nullptr);
        return texture_;
    }

    FramePtr frame(){
		last_use = std::time(nullptr);
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
