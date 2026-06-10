#include "engine/core/JJobSystem.h"

J_ENGINE_BEGIN

JJobSystem::~JJobSystem()
{
	Shutdown();
}

void JJobSystem::Initialize(uint32 workerCount)
{
	Shutdown();

	if (workerCount == 0)
	{
		const uint32 hardwareThreadCount = std::thread::hardware_concurrency();
		workerCount = hardwareThreadCount > 1 ? hardwareThreadCount - 1 : 1;
	}

	_shuttingDown = false;
	_initialized = true;
	for (uint32 i = 0; i < workerCount; ++i)
	{
		_workers.emplace_back([this]() { workerLoop(); });
	}
}

void JJobSystem::Shutdown()
{
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_shuttingDown = true;
	}

	_jobCondition.notify_all();
	for (std::thread& worker : _workers)
	{
		if (worker.joinable())
		{
			worker.join();
		}
	}

	_workers.clear();
	{
		std::lock_guard<std::mutex> lock(_mutex);
		while (!_jobs.empty())
		{
			_jobs.pop();
		}
		_activeJobs = 0;
		_initialized = false;
		_shuttingDown = false;
	}
}

void JJobSystem::Submit(Job job)
{
	if (!job)
	{
		return;
	}

	if (_workers.empty())
	{
		job();
		return;
	}

	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (_shuttingDown)
		{
			return;
		}

		_jobs.push(std::move(job));
	}
	_jobCondition.notify_one();
}

void JJobSystem::Wait()
{
	std::unique_lock<std::mutex> lock(_mutex);
	_waitCondition.wait(lock, [this]()
		{
			return _jobs.empty() && _activeJobs == 0;
		});
}

void JJobSystem::ParallelFor(uint32 count, uint32 minChunkSize, RangeJob job)
{
	if (count == 0 || !job)
	{
		return;
	}

	if (_workers.empty() || count <= minChunkSize)
	{
		job(0, count);
		return;
	}

	const uint32 workerCount = GetWorkerCount();
	const uint32 targetJobCount = std::max<uint32>(1, workerCount * 2);
	const uint32 balancedChunkSize = (count + targetJobCount - 1) / targetJobCount;
	const uint32 chunkSize = std::max<uint32>(1, std::max(minChunkSize, balancedChunkSize));

	for (uint32 begin = 0; begin < count; begin += chunkSize)
	{
		const uint32 end = std::min<uint32>(begin + chunkSize, count);
		Submit([begin, end, &job]()
			{
				job(begin, end);
			});
	}

	Wait();
}

void JJobSystem::workerLoop()
{
	while (true)
	{
		Job job;
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_jobCondition.wait(lock, [this]()
				{
					return _shuttingDown || !_jobs.empty();
				});

			if (_shuttingDown && _jobs.empty())
			{
				return;
			}

			job = std::move(_jobs.front());
			_jobs.pop();
			++_activeJobs;
		}

		job();

		{
			std::lock_guard<std::mutex> lock(_mutex);
			--_activeJobs;
			if (_jobs.empty() && _activeJobs == 0)
			{
				_waitCondition.notify_all();
			}
		}
	}
}

J_ENGINE_END

