/***

  Oak - Non-Linear Video Editor
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

#ifndef RENDERWORKERPOOL_H
#define RENDERWORKERPOOL_H

#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <deque>

#include "node/project/serializer/serializer.h"
#include "render/ipc/frameslotpool.h"
#include "render/ipc/ipcmessage.h"
#include "render/ipc/sharedmemoryregion.h"
#include "render/rendermanager.h"

namespace olive
{

class RenderWorkerPool : public QThread {
	Q_OBJECT
public:
	explicit RenderWorkerPool(QObject *parent = nullptr);
	~RenderWorkerPool() override;

	bool SubmitFrame(RenderTicketPtr ticket,
					 const RenderManager::RenderVideoParams &params);

	void Shutdown();

protected:
	void run() override;

private:
	struct Job {
		Job(RenderTicketPtr t, const RenderManager::RenderVideoParams &p)
			: ticket(t)
			, params(p)
		{
		}

		RenderTicketPtr ticket;
		RenderManager::RenderVideoParams params;
		QString graph_path;
		QString node_token;
	};

	bool PrepareJob(RenderTicketPtr ticket,
					const RenderManager::RenderVideoParams &params,
					Job *job);
	bool WriteGraphSnapshot(Project *project, QString *path);
	bool IsSupported(const RenderManager::RenderVideoParams &params) const;

	void ProcessJob(const Job &job);
	void FinishWithFrame(RenderTicketPtr ticket, const ipc::FrameSlotPool &pool,
						 uint32_t slot);
	void CleanupGraphFile(const QString &path);

	QMutex mutex_;
	QWaitCondition wait_;
	std::deque<Job> queue_;
	bool stopping_ = false;

	static constexpr uint32_t kOutputSlots = 2;
	static constexpr int kMaxWidth = 4096;
	static constexpr int kMaxHeight = 2160;
};

}

#endif // RENDERWORKERPOOL_H
