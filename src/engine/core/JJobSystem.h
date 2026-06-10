#pragma once
#ifndef __J_JOB_SYSTEM_H__
#define __J_JOB_SYSTEM_H__

#include "engine/precompile.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

J_ENGINE_BEGIN

class JJobSystem
{
public:
	using Job = std::function<void()>;
	using RangeJob = std::function<void(uint32 begin, uint32 end)>;

	JJobSystem() = default;
	~JJobSystem();

	void Initialize(uint32 workerCount = 0);
	void Shutdown();

	void Submit(Job job);
	void Wait();
	void ParallelFor(uint32 count, uint32 minChunkSize, RangeJob job);

	uint32 GetWorkerCount() const { return static_cast<uint32>(_workers.size()); }
	bool IsInitialized() const { return _initialized; }

private:
	void workerLoop();

	std::vector<std::thread> _workers;
	std::queue<Job> _jobs;
	std::mutex _mutex;
	std::condition_variable _jobCondition;
	std::condition_variable _waitCondition;
	uint32 _activeJobs = 0;
	bool _initialized = false;
	bool _shuttingDown = false;
};

J_ENGINE_END

#endif
