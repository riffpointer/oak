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
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryFile>
#include <QXmlStreamWriter>
#include <algorithm>
#include <optional>
#include <thread>
#include <vector>
#if defined(Q_OS_WIN)
#include <windows.h>
#else
#include <signal.h>
#endif

#include "codec/frame.h"
#include "common/qtutils.h"
#include "node/project/footage/footage.h"
#include "node/traverser.h"

namespace olive
{

namespace
{

constexpr int kProtocolVersion = 1;

struct FootageInput {
	FootageJob job;
	rational time;
};

class FootageInputCollector : public NodeTraverser {
public:
	QVector<FootageInput> Collect(const RenderManager::RenderVideoParams &params,
								  CancelAtom *cancel)
	{
		SetCancelPointer(cancel);
		VideoParams cache_params = params.video_params;
		cache_params.set_format(PixelFormat::F32);
		SetCacheVideoParams(cache_params);
		SetCacheAudioParams(params.audio_params);

		rational frame_length = cache_params.frame_rate_as_time_base();
		if (cache_params.interlacing() != VideoParams::kInterlaceNone) {
			frame_length /= 2;
		}
		NodeValueTable table = GenerateTable(params.node,
											 TimeRange(params.time,
													   params.time + frame_length));
		NodeValue texture = table.Get(NodeValue::kTexture);
		ResolveJobs(texture);

		if (cache_params.interlacing() != VideoParams::kInterlaceNone) {
			NodeValueTable second_table =
				GenerateTable(params.node,
							  TimeRange(params.time + frame_length,
										params.time + frame_length * 2));
			NodeValue second_texture = second_table.Get(NodeValue::kTexture);
			ResolveJobs(second_texture);
		}

		return inputs_;
	}

protected:
	void ProcessVideoFootage(TexturePtr destination,
							 const FootageJob *stream,
							 const rational &input_time) override
	{
		Q_UNUSED(destination)
		if (stream) {
			inputs_.append({*stream, input_time});
		}
	}

private:
	QVector<FootageInput> inputs_;
};

DecoderPtr ResolveDecoderFromCache(DecoderCache *decoder_cache,
								   const QString &decoder_id,
								   const Decoder::CodecStream &stream)
{
	if (!decoder_cache || !stream.IsValid()) {
		return nullptr;
	}

	QMutexLocker locker(decoder_cache->mutex());
	DecoderPair decoder = decoder_cache->value(stream);
	const qint64 file_last_modified =
		QFileInfo(stream.filename()).lastModified().toMSecsSinceEpoch();

	if (decoder.decoder && decoder.last_modified == file_last_modified) {
		return decoder.decoder;
	}

	decoder.decoder = Decoder::CreateFromID(decoder_id);
	decoder.last_modified = file_last_modified;
	decoder_cache->insert(stream, decoder);
	locker.unlock();

	if (!decoder.decoder || !decoder.decoder->Open(stream)) {
		qWarning() << "RenderWorkerPool failed to open decoder for"
				   << stream.filename() << "::" << stream.stream();
		return nullptr;
	}

	return decoder.decoder;
}

FramePtr DecodeInputFrame(DecoderCache *decoder_cache,
						  const FootageInput &input,
						  CancelAtom *cancel)
{
	VideoParams stream_data = input.job.video_params();
	QString filename = input.job.filename();
	DecoderPtr decoder;

	switch (stream_data.video_type()) {
	case VideoParams::kVideoTypeVideo:
	case VideoParams::kVideoTypeStill:
		decoder = ResolveDecoderFromCache(
			decoder_cache,
			input.job.decoder(),
			Decoder::CodecStream(filename, stream_data.stream_index(), nullptr));
		break;
	case VideoParams::kVideoTypeImageSequence: {
		const int64_t frame_number =
			stream_data.get_time_in_timebase_units(input.time);
		filename = Decoder::TransformImageSequenceFileName(filename, frame_number);
		decoder = Decoder::CreateFromID(input.job.decoder());
		if (decoder &&
			!decoder->Open(Decoder::CodecStream(filename,
												stream_data.stream_index(),
												nullptr))) {
			decoder = nullptr;
		}
		break;
	}
	}

	if (!decoder) {
		return nullptr;
	}

	Decoder::RetrieveVideoParams retrieve;
	retrieve.divider = stream_data.divider();
	retrieve.maximum_format = PixelFormat::U16;
	retrieve.time = stream_data.video_type() == VideoParams::kVideoTypeVideo
						? input.time
						: Decoder::kAnyTimecode;
	retrieve.cancelled = cancel;
	retrieve.force_range = stream_data.color_range();
	retrieve.src_interlacing = stream_data.interlacing();
	FramePtr frame = decoder->RetrieveVideoFrame(retrieve);
	if (frame) {
		frame->set_timestamp(input.time);
	}
	return frame;
}

bool DecodeInputFrames(DecoderCache *decoder_cache,
					   const RenderManager::RenderVideoParams &params,
					   CancelAtom *cancel,
					   QVector<FramePtr> *frames)
{
	frames->clear();

	FootageInputCollector collector;
	const QVector<FootageInput> inputs = collector.Collect(params, cancel);
	frames->reserve(inputs.size());
	for (const FootageInput &input : inputs) {
		if (cancel && cancel->IsCancelled()) {
			return false;
		}

		FramePtr frame = DecodeInputFrame(decoder_cache, input, cancel);
		if (!frame || !frame->is_allocated()) {
			frames->clear();
			return false;
		}
		frames->append(frame);
	}

	return true;
}

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

void TryWriteControlMessage(QProcess *process, const QJsonObject &obj)
{
	const QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
	process->write(line);
}

bool KillProcessById(qint64 process_id)
{
	if (process_id <= 0) {
		return false;
	}

#if defined(Q_OS_WIN)
	HANDLE handle = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(process_id));
	if (!handle) {
		return false;
	}
	const bool ok = TerminateProcess(handle, 1) != 0;
	CloseHandle(handle);
	return ok;
#else
	return ::kill(pid_t(process_id), SIGKILL) == 0;
#endif
}

QString WorkerProcessDetails(const QProcess *process)
{
	if (!process) {
		return QStringLiteral("worker process unavailable");
	}

	const QString exit_status =
		process->exitStatus() == QProcess::CrashExit
			? QStringLiteral("crash")
			: QStringLiteral("normal");
	return QStringLiteral("state=%1 exit_status=%2 exit_code=%3 process_error=%4 error=\"%5\"")
		.arg(int(process->state()))
		.arg(exit_status)
		.arg(process->exitCode())
		.arg(int(process->error()))
		.arg(process->errorString());
}

bool ReadControlMessage(QProcess *process, QJsonObject *out, QString *error,
						int timeout_ms = 10000)
{
	if (!process->waitForReadyRead(timeout_ms)) {
		if (error) {
			if (process->state() == QProcess::NotRunning) {
				*error = QStringLiteral("worker exited before response: %1")
							 .arg(WorkerProcessDetails(process));
			} else {
				*error = QStringLiteral("timeout waiting for worker response: %1")
							 .arg(WorkerProcessDetails(process));
			}
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

RenderWorkerPool::RenderWorkerPool(DecoderCache *decoder_cache,
								   QObject *parent)
	: QThread(parent)
	, decoder_cache_(decoder_cache)
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

bool RenderWorkerPool::RemoveTicket(RenderTicketPtr ticket)
{
	if (!ticket) {
		return false;
	}

	QString queued_graph_path;
	bool matched_active = false;

	{
		QMutexLocker locker(&mutex_);
		auto it = std::find_if(queue_.begin(), queue_.end(),
							   [&ticket](const Job &job) {
								   return job.ticket == ticket;
							   });
		if (it != queue_.end()) {
			queued_graph_path = it->graph_path;
			queue_.erase(it);
		} else {
			for (const ActiveJob &active : active_jobs_) {
				if (active.ticket == ticket) {
					ticket->Cancel();
					CancelActiveProcess(active.process_id);
					matched_active = true;
					break;
				}
			}
			if (!matched_active) {
				return false;
			}
		}
	}

	if (!queued_graph_path.isEmpty()) {
		CleanupGraphFile(queued_graph_path);
		return true;
	}

	return true;
}

void RenderWorkerPool::Shutdown()
{
	QVector<QString> queued_graph_paths;

	{
		QMutexLocker locker(&mutex_);
		stopping_ = true;
		for (Job &job : queue_) {
			if (job.ticket) {
				job.ticket->Cancel();
			}
			queued_graph_paths.append(job.graph_path);
		}
		queue_.clear();
		for (ActiveJob &active : active_jobs_) {
			if (active.ticket) {
				active.ticket->Cancel();
				CancelActiveProcess(active.process_id);
			}
		}
		wait_.wakeAll();
	}

	for (const QString &path : queued_graph_paths) {
		CleanupGraphFile(path);
	}

	if (isRunning()) {
		wait();
	}
}

void RenderWorkerPool::run()
{
	const int worker_count = WorkerCount();
	{
		QMutexLocker locker(&mutex_);
		active_jobs_.resize(worker_count);
	}

	std::vector<std::thread> workers;
	workers.reserve(size_t(worker_count));
	for (int i = 0; i < worker_count; i++) {
		workers.emplace_back([this, i]() {
			WorkerLoop(i);
		});
	}

	for (std::thread &worker : workers) {
		worker.join();
	}

	QMutexLocker locker(&mutex_);
	active_jobs_.clear();
}

void RenderWorkerPool::WorkerLoop(int worker_index)
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

		ProcessJob(job, worker_index);
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

	QVector<FramePtr> input_frames;
	if (!DecodeInputFrames(decoder_cache_, params, ticket->GetCancelAtom(),
						   &input_frames)) {
		qWarning() << "RenderWorkerPool could not predecode footage inputs;"
				   << "falling back to in-process render";
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
	job->input_frames = input_frames;
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

void RenderWorkerPool::ProcessJob(const Job &job, int worker_index)
{
	const qint64 ticket_id = qint64(reinterpret_cast<quintptr>(job.ticket.get()));
	SetActiveWorker(worker_index, job.ticket, nullptr, ticket_id);

	job.ticket->Start();
	if (job.ticket->IsCancelled()) {
		job.ticket->Finish();
		ClearActiveWorker(worker_index, 0);
		return;
	}

	for (int attempt = 0; attempt < kMaxAttempts; attempt++) {
		const JobResult result = ProcessJobAttempt(job, worker_index, attempt);
		if (result == JobResult::kFinished) {
			ClearActiveWorker(worker_index, 0);
			return;
		}
		if (result == JobResult::kCancelled) {
			job.ticket->Finish();
			ClearActiveWorker(worker_index, 0);
			return;
		}
		if (result == JobResult::kFatalFailure) {
			break;
		}
		if (attempt + 1 < kMaxAttempts && !job.ticket->IsCancelled()) {
			qWarning() << "RenderWorkerPool retrying render worker for ticket"
					   << ticket_id << "after worker failure";
		}
	}

	if (job.ticket->IsCancelled()) {
		job.ticket->Finish();
	} else {
		qWarning() << "RenderWorkerPool exhausted worker retries for ticket"
				   << ticket_id;
		job.ticket->Finish();
	}
	ClearActiveWorker(worker_index, 0);
}

RenderWorkerPool::JobResult RenderWorkerPool::ProcessJobAttempt(
	const Job &job, int worker_index, int attempt_index)
{
	const qint64 ticket_id = qint64(reinterpret_cast<quintptr>(job.ticket.get()));
	if (job.ticket->IsCancelled()) {
		return JobResult::kCancelled;
	}

	const int linesize = Frame::generate_linesize_bytes(
		kMaxWidth, PixelFormat::F32, VideoParams::kRGBAChannelCount);
	const size_t slot_bytes = size_t(linesize) * kMaxHeight;
	const size_t region_bytes = ipc::FrameSlotPool::BytesNeeded(kOutputSlots, slot_bytes);
	const QString shm_key =
		ipc::SharedMemoryRegion::MakeKey(QCoreApplication::applicationPid(),
										 int((reinterpret_cast<quintptr>(job.ticket.get()) +
											  attempt_index * 2) & 0xFFFF));

	ipc::SharedMemoryRegion region;
	if (!region.Open(shm_key, region_bytes, ipc::SharedMemoryRegion::kCreate)) {
		qWarning() << "RenderWorkerPool failed to create shared memory"
				   << region.error();
		return JobResult::kFatalFailure;
	}
	ipc::FrameSlotPool output_pool =
		ipc::FrameSlotPool::Create(region.data(), kOutputSlots, slot_bytes);

	const QString input_shm_key =
		job.input_frames.isEmpty()
			? QString()
			: ipc::SharedMemoryRegion::MakeKey(
				  QCoreApplication::applicationPid(),
				  int((reinterpret_cast<quintptr>(job.ticket.get()) +
					   attempt_index * 2 + 1) & 0xFFFF));
	ipc::SharedMemoryRegion input_region;
	std::optional<ipc::FrameSlotPool> input_pool;
	QVector<int> input_slots;
	if (!job.input_frames.isEmpty()) {
		const uint32_t input_slot_count = uint32_t(job.input_frames.size());
		const size_t input_region_bytes =
			ipc::FrameSlotPool::BytesNeeded(input_slot_count, slot_bytes);
		if (!input_region.Open(input_shm_key, input_region_bytes,
							   ipc::SharedMemoryRegion::kCreate)) {
			qWarning() << "RenderWorkerPool failed to create input shared memory"
					   << input_region.error();
			return JobResult::kFatalFailure;
		} else {
			input_pool = ipc::FrameSlotPool::Create(input_region.data(),
													input_slot_count,
													slot_bytes);
			for (const FramePtr &frame : job.input_frames) {
				if (frame->allocated_size() > int(slot_bytes)) {
					qWarning() << "RenderWorkerPool decoded input frame exceeds slot size";
					return JobResult::kFatalFailure;
				}

				uint32_t slot = 0;
				if (!input_pool->Acquire(&slot)) {
					qWarning() << "RenderWorkerPool input pool had no free slot";
					return JobResult::kFatalFailure;
				}

				memcpy(input_pool->SlotData(slot), frame->const_data(),
					   size_t(frame->allocated_size()));
				ipc::FrameSlotMeta *meta = input_pool->Meta(slot);
				meta->id = qint64(input_slots.size());
				meta->time_num = frame->timestamp().numerator();
				meta->time_den = frame->timestamp().denominator();
				meta->width = frame->width();
				meta->height = frame->height();
				meta->format = int32_t(frame->format());
				meta->channel_count = frame->channel_count();
				meta->linesize = frame->linesize_bytes();
				meta->data_size = frame->allocated_size();
				if (!input_pool->Publish(slot)) {
					qWarning() << "RenderWorkerPool failed to publish input slot";
					return JobResult::kFatalFailure;
				}
				input_slots.append(int(slot));
			}

			if (input_slots.size() != job.input_frames.size()) {
				qWarning() << "RenderWorkerPool failed to publish all input frames;"
						   << "aborting worker render";
				return JobResult::kFatalFailure;
			}
		}
	}

	QProcess worker;
	worker.setProgram(WorkerProgramPath());
	worker.start();
	if (!worker.waitForStarted(10000)) {
		qWarning() << "RenderWorkerPool failed to start worker"
				   << worker.errorString();
		return JobResult::kRetryableFailure;
	}
	const qint64 worker_process_id = worker.processId();

	SetActiveWorker(worker_index, job.ticket, &worker, ticket_id);
	if (job.ticket->IsCancelled()) {
		ipc::CancelMsg cancel;
		cancel.ticket_id = ticket_id;
		TryWriteControlMessage(&worker, cancel.ToJson());
		worker.kill();
		worker.waitForFinished();
		ClearActiveWorker(worker_index, worker_process_id);
		return JobResult::kCancelled;
	}

	QString error;
	QJsonObject response;
	if (!ReadControlMessage(&worker, &response, &error)) {
		if (!job.ticket->IsCancelled()) {
			qWarning() << "RenderWorkerPool did not receive startup handshake"
					   << error << worker.readAllStandardError();
		}
		worker.kill();
		worker.waitForFinished();
		ClearActiveWorker(worker_index, worker_process_id);
		return job.ticket->IsCancelled() ? JobResult::kCancelled
										 : JobResult::kRetryableFailure;
	}

	ipc::HandshakeMsg handshake;
	handshake.protocol_version = kProtocolVersion;
	handshake.shm_key = shm_key;
	handshake.input_shm_key = input_slots.isEmpty() ? QString() : input_shm_key;
	handshake.input_slots = input_slots.size();
	handshake.output_slots = int(kOutputSlots);
	handshake.slot_data_bytes = qint64(slot_bytes);
	handshake.input_slot_data_bytes = input_slots.isEmpty() ? 0 : qint64(slot_bytes);
	if (!WriteControlMessage(&worker, handshake.ToJson())) {
		if (!job.ticket->IsCancelled()) {
			qWarning() << "RenderWorkerPool failed to send shared-memory handshake";
		}
		worker.kill();
		worker.waitForFinished();
		ClearActiveWorker(worker_index, worker_process_id);
		return job.ticket->IsCancelled() ? JobResult::kCancelled
										 : JobResult::kRetryableFailure;
	}

	ipc::LoadGraphMsg load;
	load.path = job.graph_path;
	if (!WriteControlMessage(&worker, load.ToJson()) ||
		!ReadControlMessage(&worker, &response, &error)) {
		if (!job.ticket->IsCancelled()) {
			qWarning() << "RenderWorkerPool failed to load graph in worker"
					   << error << worker.readAllStandardError();
		}
		worker.kill();
		worker.waitForFinished();
		ClearActiveWorker(worker_index, worker_process_id);
		return job.ticket->IsCancelled() ? JobResult::kCancelled
										 : JobResult::kRetryableFailure;
	}

	ipc::RenderFrameMsg render;
	render.ticket_id = ticket_id;
	render.node_uuid = job.node_token;
	render.time_num = job.params.time.numerator();
	render.time_den = job.params.time.denominator();
	render.width = job.params.force_size.width();
	render.height = job.params.force_size.height();
	render.format = int(job.params.force_format);
	render.channel_count = job.params.force_channel_count;
	render.mode = int(job.params.mode);
	render.input_slot = input_slots.isEmpty() ? -1 : input_slots.front();
	render.input_slots = input_slots;

	if (!WriteControlMessage(&worker, render.ToJson())) {
		if (!job.ticket->IsCancelled()) {
			qWarning() << "RenderWorkerPool failed to send render_frame";
		}
		worker.kill();
		worker.waitForFinished();
		ClearActiveWorker(worker_index, worker_process_id);
		return job.ticket->IsCancelled() ? JobResult::kCancelled
										 : JobResult::kRetryableFailure;
	}

	ipc::FrameReadyMsg ready;
	while (true) {
		if (!ReadControlMessage(&worker, &response, &error, 30000)) {
			if (!job.ticket->IsCancelled()) {
				qWarning() << "RenderWorkerPool failed waiting for frame_ready"
						   << error << worker.readAllStandardError();
			}
			worker.kill();
			worker.waitForFinished();
			ClearActiveWorker(worker_index, worker_process_id);
			return job.ticket->IsCancelled() ? JobResult::kCancelled
											 : JobResult::kRetryableFailure;
		}

		if (ipc::FrameReadyMsg::FromJson(response, &ready)) {
			break;
		}
	}

	if (job.ticket->IsCancelled()) {
		ClearActiveWorker(worker_index, worker_process_id);
		return JobResult::kCancelled;
	} else {
		FinishWithFrame(job.ticket, output_pool, uint32_t(ready.output_slot));
	}
	ClearActiveWorker(worker_index, worker_process_id);

	QJsonObject shutdown;
	shutdown[QStringLiteral("type")] = ipc::msgtype::kShutdown;
	WriteControlMessage(&worker, shutdown);
	worker.closeWriteChannel();
	if (!worker.waitForFinished(5000)) {
		worker.kill();
		worker.waitForFinished();
	}
	return JobResult::kFinished;
}

void RenderWorkerPool::CancelActiveProcess(qint64 process_id)
{
	KillProcessById(process_id);
}

void RenderWorkerPool::SetActiveWorker(int worker_index, RenderTicketPtr ticket,
									   QProcess *worker, qint64 ticket_id)
{
	QMutexLocker locker(&mutex_);
	if (worker_index < 0 || worker_index >= active_jobs_.size()) {
		return;
	}

	ActiveJob &active = active_jobs_[worker_index];
	active.ticket = ticket;
	active.process_id = worker ? worker->processId() : 0;
	active.ticket_id = ticket_id;
}

void RenderWorkerPool::ClearActiveWorker(int worker_index, qint64 process_id)
{
	QMutexLocker locker(&mutex_);
	if (worker_index < 0 || worker_index >= active_jobs_.size()) {
		return;
	}

	ActiveJob &active = active_jobs_[worker_index];
	if (process_id > 0) {
		if (active.process_id == process_id) {
			active.process_id = 0;
		}
	} else {
		active = ActiveJob();
	}
}

int RenderWorkerPool::WorkerCount() const
{
	const int ideal = QThread::idealThreadCount();
	return std::max(1, ideal - 2);
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
