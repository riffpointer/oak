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

#include "renderworkerpool.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

#include "codec/frame.h"
#include "common/qtutils.h"

namespace olive
{

namespace
{

constexpr int kProtocolVersion = 1;

QString WorkerProgramPath()
{
	const QString dir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_WIN)
	return QDir(dir).filePath(QStringLiteral("olive-render-worker.exe"));
#else
	return QDir(dir).filePath(QStringLiteral("olive-render-worker"));
#endif
}

bool WriteControlMessage(QProcess *process, const QJsonObject &obj)
{
	const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
	return process->write(line) == line.size() && process->waitForBytesWritten(5000);
}

bool ReadControlMessage(QProcess *process, QJsonObject *out, QString *error,
						int timeout_ms = 10000)
{
	if (!process->waitForReadyRead(timeout_ms)) {
		if (error) {
			*error = QStringLiteral("timeout waiting for worker response");
		}
		return false;
	}

	while (process->canReadLine()) {
		const QByteArray line = process->readLine().trimmed();
		if (line.isEmpty()) {
			continue;
		}

		QJsonParseError parse_error;
		const QJsonDocument doc = QJsonDocument::fromJson(line, &parse_error);
		if (parse_error.error != QJsonParseError::NoError || !doc.isObject()) {
			if (error) {
				*error = QStringLiteral("worker emitted malformed control JSON");
			}
			return false;
		}

		*out = doc.object();
		if (out->value(QStringLiteral("type")).toString() ==
			QLatin1String(ipc::msgtype::kError)) {
			if (error) {
				*error = out->value(QStringLiteral("message")).toString();
			}
			return false;
		}
		return true;
	}

	if (error) {
		*error = QStringLiteral("worker response did not contain a full line");
	}
	return false;
}

}  // namespace

RenderWorkerPool::RenderWorkerPool(QObject *parent)
	: QThread(parent)
{
}

RenderWorkerPool::~RenderWorkerPool()
{
	Shutdown();
}

bool RenderWorkerPool::SubmitFrame(RenderTicketPtr ticket,
								   const RenderManager::RenderVideoParams &params)
{
	Job job(ticket, params);
	if (!PrepareJob(ticket, params, &job)) {
		return false;
	}

	ticket->moveToThread(this);

	QMutexLocker locker(&mutex_);
	queue_.push_back(job);
	wait_.wakeOne();
	return true;
}

void RenderWorkerPool::Shutdown()
{
	{
		QMutexLocker locker(&mutex_);
		stopping_ = true;
		wait_.wakeOne();
	}

	if (isRunning()) {
		wait();
	}
}

void RenderWorkerPool::run()
{
	while (true) {
		mutex_.lock();
		while (queue_.empty() && !stopping_) {
			wait_.wait(&mutex_);
		}
		if (stopping_ && queue_.empty()) {
			mutex_.unlock();
			break;
		}

		Job job = queue_.front();
		queue_.pop_front();
		mutex_.unlock();

		ProcessJob(job);
		CleanupGraphFile(job.graph_path);
	}
}

bool RenderWorkerPool::PrepareJob(RenderTicketPtr ticket,
								  const RenderManager::RenderVideoParams &params,
								  Job *job)
{
	if (!IsSupported(params)) {
		return false;
	}

	Project *project = Project::GetProjectFromObject(params.node);
	if (!project) {
		qWarning() << "RenderWorkerPool could not resolve project for render node";
		return false;
	}

	QString graph_path;
	if (!WriteGraphSnapshot(project, &graph_path)) {
		return false;
	}

	job->ticket = ticket;
	job->params = params;
	job->graph_path = graph_path;
	job->node_token = QString::number(reinterpret_cast<quintptr>(params.node));
	return true;
}

bool RenderWorkerPool::WriteGraphSnapshot(Project *project, QString *path)
{
	QTemporaryFile file(QDir::temp().filePath(QStringLiteral("oak-render-graph-XXXXXX.ove")));
	file.setAutoRemove(false);
	if (!file.open()) {
		qWarning() << "RenderWorkerPool failed to create graph snapshot temp file"
				   << file.errorString();
		return false;
	}

	QXmlStreamWriter writer(&file);
	ProjectSerializer::SaveData data(ProjectSerializer::kProject, project, file.fileName());
	const ProjectSerializer::Result result = ProjectSerializer::Save(&writer, data);
	file.close();

	if (result.code() != ProjectSerializer::kSuccess || writer.hasError()) {
		qWarning() << "RenderWorkerPool failed to serialize graph snapshot"
				   << result.GetDetails();
		QFile::remove(file.fileName());
		return false;
	}

	*path = file.fileName();
	return true;
}

bool RenderWorkerPool::IsSupported(const RenderManager::RenderVideoParams &params) const
{
	return params.node && params.return_type == RenderManager::kFrame &&
		   params.video_params.is_valid();
}

void RenderWorkerPool::ProcessJob(const Job &job)
{
	job.ticket->Start();
	if (job.ticket->IsCancelled()) {
		job.ticket->Finish();
		return;
	}

	const int linesize = Frame::generate_linesize_bytes(
		kMaxWidth, PixelFormat::F32, VideoParams::kRGBAChannelCount);
	const size_t slot_bytes = size_t(linesize) * kMaxHeight;
	const size_t region_bytes = ipc::FrameSlotPool::BytesNeeded(kOutputSlots, slot_bytes);
	const QString shm_key =
		ipc::SharedMemoryRegion::MakeKey(QCoreApplication::applicationPid(),
										 int(reinterpret_cast<quintptr>(job.ticket.get()) & 0xFFFF));

	ipc::SharedMemoryRegion region;
	if (!region.Open(shm_key, region_bytes, ipc::SharedMemoryRegion::kCreate)) {
		qWarning() << "RenderWorkerPool failed to create shared memory"
				   << region.error();
		job.ticket->Finish();
		return;
	}
	ipc::FrameSlotPool output_pool =
		ipc::FrameSlotPool::Create(region.data(), kOutputSlots, slot_bytes);

	QProcess worker;
	worker.setProgram(WorkerProgramPath());
	worker.start();
	if (!worker.waitForStarted(10000)) {
		qWarning() << "RenderWorkerPool failed to start worker"
				   << worker.errorString();
		job.ticket->Finish();
		return;
	}

	QString error;
	QJsonObject response;
	if (!ReadControlMessage(&worker, &response, &error)) {
		qWarning() << "RenderWorkerPool did not receive startup handshake"
				   << error << worker.readAllStandardError();
		worker.kill();
		worker.waitForFinished();
		job.ticket->Finish();
		return;
	}

	ipc::HandshakeMsg handshake;
	handshake.protocol_version = kProtocolVersion;
	handshake.shm_key = shm_key;
	handshake.input_slots = 0;
	handshake.output_slots = int(kOutputSlots);
	handshake.slot_data_bytes = qint64(slot_bytes);
	if (!WriteControlMessage(&worker, handshake.ToJson())) {
		qWarning() << "RenderWorkerPool failed to send shared-memory handshake";
		worker.kill();
		worker.waitForFinished();
		job.ticket->Finish();
		return;
	}

	ipc::LoadGraphMsg load;
	load.path = job.graph_path;
	if (!WriteControlMessage(&worker, load.ToJson()) ||
		!ReadControlMessage(&worker, &response, &error)) {
		qWarning() << "RenderWorkerPool failed to load graph in worker"
				   << error << worker.readAllStandardError();
		worker.kill();
		worker.waitForFinished();
		job.ticket->Finish();
		return;
	}

	ipc::RenderFrameMsg render;
	render.ticket_id = qint64(reinterpret_cast<quintptr>(job.ticket.get()));
	render.node_uuid = job.node_token;
	render.time_num = job.params.time.numerator();
	render.time_den = job.params.time.denominator();
	render.width = job.params.force_size.width();
	render.height = job.params.force_size.height();
	render.format = int(job.params.force_format);
	render.channel_count = job.params.force_channel_count;
	render.mode = int(job.params.mode);

	if (!WriteControlMessage(&worker, render.ToJson())) {
		qWarning() << "RenderWorkerPool failed to send render_frame";
		worker.kill();
		worker.waitForFinished();
		job.ticket->Finish();
		return;
	}

	ipc::FrameReadyMsg ready;
	while (true) {
		if (!ReadControlMessage(&worker, &response, &error, 30000)) {
			qWarning() << "RenderWorkerPool failed waiting for frame_ready"
					   << error << worker.readAllStandardError();
			worker.kill();
			worker.waitForFinished();
			job.ticket->Finish();
			return;
		}

		if (ipc::FrameReadyMsg::FromJson(response, &ready)) {
			break;
		}
	}

	FinishWithFrame(job.ticket, output_pool, uint32_t(ready.output_slot));

	QJsonObject shutdown;
	shutdown[QStringLiteral("type")] = ipc::msgtype::kShutdown;
	WriteControlMessage(&worker, shutdown);
	worker.closeWriteChannel();
	if (!worker.waitForFinished(5000)) {
		worker.kill();
		worker.waitForFinished();
	}
}

void RenderWorkerPool::FinishWithFrame(RenderTicketPtr ticket,
									   const ipc::FrameSlotPool &pool,
									   uint32_t slot)
{
	const ipc::FrameSlotMeta *meta = pool.Meta(slot);
	if (!meta || meta->data_size <= 0 ||
		meta->data_size > int(pool.slot_data_bytes())) {
		ticket->Finish();
		return;
	}

	VideoParams params(meta->width, meta->height,
					   PixelFormat::Format(meta->format),
					   meta->channel_count);
	FramePtr frame = Frame::Create();
	frame->set_timestamp(rational(int(meta->time_num), int(meta->time_den)));
	frame->set_video_params(params);
	if (!frame->allocate() || frame->allocated_size() < meta->data_size) {
		ticket->Finish();
		return;
	}

	memcpy(frame->data(), pool.SlotData(slot), size_t(meta->data_size));
	ticket->Finish(QVariant::fromValue(frame));
}

void RenderWorkerPool::CleanupGraphFile(const QString &path)
{
	if (!path.isEmpty()) {
		QFile::remove(path);
	}
}

}  // namespace olive
