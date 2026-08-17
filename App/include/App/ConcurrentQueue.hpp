#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace App
{
	template<typename T>
	class ConcurrentQueue final
	{
	public:
		void Push( T value )
		{
			std::scoped_lock lock( mutex_ );
			queue_.push( std::move( value ) );
		}

		std::optional<T> TryPop()
		{
			std::scoped_lock lock( mutex_ );
			if( queue_.empty() )
			{
				return std::nullopt;
			}

			T value = std::move( queue_.front() );
			queue_.pop();
			return value;
		}

	private:
		std::mutex mutex_;
		std::queue<T> queue_;
	};
}
