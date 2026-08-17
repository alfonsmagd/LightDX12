#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace enki
{
	class TaskScheduler;
	class TaskSet;
}

namespace App
{
	enum class JobState : uint8_t
	{
		Queued,
		Running,
		Completed,
		Failed
	};

	struct JobSnapshot
	{
		uint64_t id = 0;
		std::string name;
		JobState state = JobState::Queued;
		std::string error;
		double elapsedMilliseconds = 0.0;
	};

	class TaskSystem final
	{
	public:
		explicit TaskSystem( uint32_t workerCount = 0 );
		~TaskSystem();

		TaskSystem( const TaskSystem& ) = delete;
		TaskSystem& operator=( const TaskSystem& ) = delete;

		uint64_t Submit( std::string name, std::function<void()> function );
		std::vector<JobSnapshot> Snapshot() const;
		uint32_t WorkerCount() const noexcept;
		uint32_t PendingCount() const noexcept;
		uint32_t ActiveCount() const noexcept;
		void WaitIdle();

	private:
		struct JobRecord;

		std::unique_ptr<enki::TaskScheduler> scheduler_;
		mutable std::mutex mutex_;
		std::vector<std::shared_ptr<JobRecord>> records_;
		std::vector<std::unique_ptr<enki::TaskSet>> tasks_;
		std::atomic<uint64_t> nextId_{ 1 };
		std::atomic<uint32_t> pending_{ 0 };
		std::atomic<uint32_t> active_{ 0 };
		uint32_t workerCount_ = 0;
	};
}
