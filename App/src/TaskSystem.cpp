#include "App/TaskSystem.hpp"

#include "TaskScheduler.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

namespace App
{
	struct TaskSystem::JobRecord
	{
		uint64_t id = 0;
		std::string name;
		std::atomic<JobState> state{ JobState::Queued };
		std::mutex errorMutex;
		std::string error;
		std::chrono::steady_clock::time_point submitted = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point started{};
		std::chrono::steady_clock::time_point finished{};
	};

	TaskSystem::TaskSystem( uint32_t workerCount ):
		scheduler_( std::make_unique<enki::TaskScheduler>() )
	{
		enki::TaskSchedulerConfig config;
		if( workerCount > 0 )
		{
			config.numTaskThreadsToCreate = workerCount;
		}
		scheduler_->Initialize( config );
		workerCount_ = scheduler_->GetNumTaskThreads();
	}

	TaskSystem::~TaskSystem()
	{
		if( scheduler_ )
		{
			scheduler_->WaitforAllAndShutdown();
		}
	}

	uint64_t TaskSystem::Submit( std::string name, std::function<void()> function )
	{
		if( !function )
		{
			throw std::invalid_argument( "TaskSystem::Submit requires a function." );
		}

		auto record = std::make_shared<JobRecord>();
		record->id = nextId_.fetch_add( 1 );
		record->name = std::move( name );
		pending_.fetch_add( 1 );

		auto task = std::make_unique<enki::TaskSet>( 1, [this, record, function = std::move( function )]( enki::TaskSetPartition, uint32_t )
		{
			pending_.fetch_sub( 1 );
			active_.fetch_add( 1 );
			record->started = std::chrono::steady_clock::now();
			record->state.store( JobState::Running );
			JobState terminalState = JobState::Completed;
			try
			{
				function();
			}
			catch( const std::exception& exception )
			{
				{
					std::scoped_lock lock( record->errorMutex );
					record->error = exception.what();
				}
				terminalState = JobState::Failed;
			}
			catch( ... )
			{
				{
					std::scoped_lock lock( record->errorMutex );
					record->error = "Unknown job error.";
				}
				terminalState = JobState::Failed;
			}
			record->finished = std::chrono::steady_clock::now();
			record->state.store( terminalState );
			active_.fetch_sub( 1 );
		} );

		enki::TaskSet* taskPointer = task.get();
		{
			std::scoped_lock lock( mutex_ );
			records_.push_back( record );
			tasks_.push_back( std::move( task ) );
		}
		scheduler_->AddTaskSetToPipe( taskPointer );
		return record->id;
	}

	std::vector<JobSnapshot> TaskSystem::Snapshot() const
	{
		std::vector<std::shared_ptr<JobRecord>> records;
		{
			std::scoped_lock lock( mutex_ );
			records = records_;
		}

		std::vector<JobSnapshot> result;
		result.reserve( records.size() );
		const auto now = std::chrono::steady_clock::now();
		for( const auto& record : records )
		{
			JobSnapshot snapshot;
			snapshot.id = record->id;
			snapshot.name = record->name;
			snapshot.state = record->state.load();
			{
				std::scoped_lock lock( record->errorMutex );
				snapshot.error = record->error;
			}
			const auto begin = snapshot.state == JobState::Queued ? record->submitted : record->started;
			const auto end = ( snapshot.state == JobState::Completed || snapshot.state == JobState::Failed ) ? record->finished : now;
			snapshot.elapsedMilliseconds = std::chrono::duration<double, std::milli>( end - begin ).count();
			result.push_back( std::move( snapshot ) );
		}
		return result;
	}

	uint32_t TaskSystem::WorkerCount() const noexcept { return workerCount_; }
	uint32_t TaskSystem::PendingCount() const noexcept { return pending_.load(); }
	uint32_t TaskSystem::ActiveCount() const noexcept { return active_.load(); }

	void TaskSystem::WaitIdle()
	{
		scheduler_->WaitforAll();
	}
}
