#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ldx12
{
	template<typename ResourceType>
	class Handle final
	{
	public:
		Handle() = default;

		// Reports whether this is a non-empty handle token. Use SlotMap::Contains()
		// when the current lifetime/generation must also be validated.
		bool Valid() const noexcept
		{
			return gen_ != 0;
		}

		uint32_t Index() const noexcept
		{
			return index_;
		}

		uint32_t Gen() const noexcept
		{
			return gen_;
		}

		bool Empty() const noexcept
		{
			return gen_ == 0;
		}

		bool operator==( const Handle& other ) const noexcept
		{
			return index_ == other.index_ && gen_ == other.gen_;
		}

		bool operator!=( const Handle& other ) const noexcept
		{
			return !(*this == other);
		}

		explicit operator bool() const noexcept
		{
			return gen_ != 0;
		}

		operator uint32_t() const noexcept
		{
			return gen_ != 0 ? index_ : 0x00FFFFFFu;
		}

	private:
		Handle( uint32_t index, uint32_t gen ) noexcept: index_( index ), gen_( gen ) {}

		template<typename T, std::size_t Capacity>
		friend class SlotMap;

		uint32_t index_ = 0;
		uint32_t gen_ = 0;
	};

	template<typename ObjectType, std::size_t Capacity>
	class SlotMap final
	{
		static_assert( Capacity > 0 );
		static_assert( Capacity <= std::numeric_limits<uint32_t>::max() );

		struct Slot
		{
			ObjectType obj = {};
			uint32_t gen = 1;
			bool occupied = false;
		};

	public:
		Handle<ObjectType> Create( ObjectType&& obj )
		{
			uint32_t index = 0;
			if( freeCount_ > 0 )
			{
				index = freeList_[ freeCount_ - 1 ];
				slots_[ index ].obj = std::move( obj );
				--freeCount_;
				slots_[ index ].occupied = true;
			}
			else
			{
				if( slotCount_ == Capacity )
				{
					throw std::length_error( "Ldx12 SlotMap capacity exhausted." );
				}

				index = slotCount_;
				slots_[ index ].obj = std::move( obj );
				slots_[ index ].occupied = true;
				++slotCount_;
			}

			++objectCount_;
			return Handle<ObjectType>( index, slots_[ index ].gen );
		}

		bool Contains( Handle<ObjectType> handle ) const noexcept
		{
			if( !handle.Valid() )
			{
				return false;
			}

			const uint32_t index = handle.Index();
			if( index >= slotCount_ )
			{
				return false;
			}

			const Slot& slot = slots_[ index ];
			return slot.occupied && handle.Gen() == slot.gen;
		}

		bool Destroy( Handle<ObjectType> handle )
		{
			if( !Contains( handle ) )
			{
				return false;
			}

			const uint32_t index = handle.Index();
			Slot& slot = slots_[ index ];
			slot.obj = {};
			IncrementGeneration( slot );
			slot.occupied = false;
			freeList_[ freeCount_++ ] = index;
			--objectCount_;
			return true;
		}

		ObjectType* Get( Handle<ObjectType> handle )
		{
			return Contains( handle ) ? &slots_[ handle.Index() ].obj : nullptr;
		}

		const ObjectType* Get( Handle<ObjectType> handle ) const
		{
			return Contains( handle ) ? &slots_[ handle.Index() ].obj : nullptr;
		}

		ObjectType* GetByIndex( uint32_t index )
		{
			if( index >= slotCount_ )
			{
				return nullptr;
			}

			auto& slot = slots_[ index ];
			if( !slot.occupied )
			{
				return nullptr;
			}

			return &slot.obj;
		}

		const ObjectType* GetByIndex( uint32_t index ) const
		{
			if( index >= slotCount_ )
			{
				return nullptr;
			}

			const auto& slot = slots_[ index ];
			if( !slot.occupied )
			{
				return nullptr;
			}

			return &slot.obj;
		}

		Handle<ObjectType> Find( const ObjectType* obj ) const
		{
			if( obj == nullptr )
			{
				return {};
			}

			for( uint32_t i = 0; i < slotCount_; ++i )
			{
				if( slots_[ i ].occupied && &slots_[ i ].obj == obj )
				{
					return Handle<ObjectType>( i, slots_[ i ].gen );
				}
			}

			return {};
		}

		uint32_t NumObjects() const noexcept
		{
			return objectCount_;
		}

		void Clear()
		{
			for( uint32_t i = 0; i < slotCount_; ++i )
			{
				Slot& slot = slots_[ i ];
				if( slot.occupied )
				{
					slot.obj = {};
					IncrementGeneration( slot );
					slot.occupied = false;
				}
			}

			slotCount_ = 0;
			freeCount_ = 0;
			objectCount_ = 0;
		}

		template<typename Function>
		void ForEach( Function&& function )
		{
			for( uint32_t i = 0; i < slotCount_; ++i )
			{
				Slot& slot = slots_[ i ];
				if( slot.occupied )
				{
					std::forward<Function>( function )( slot.obj );
				}
			}
		}

		template<typename Function>
		void ForEach( Function&& function ) const
		{
			for( uint32_t i = 0; i < slotCount_; ++i )
			{
				const Slot& slot = slots_[ i ];
				if( slot.occupied )
				{
					std::forward<Function>( function )( slot.obj );
				}
			}
		}

		uint32_t Size() const noexcept
		{
			return objectCount_;
		}

		bool Empty() const noexcept
		{
			return objectCount_ == 0;
		}

		static constexpr std::size_t MaxSize() noexcept
		{
			return Capacity;
		}

	private:
		static void IncrementGeneration( Slot& slot ) noexcept
		{
			++slot.gen;
			if( slot.gen == 0 )
			{
				slot.gen = 1;
			}
		}

		std::array<Slot, Capacity> slots_ = {};
		std::array<uint32_t, Capacity> freeList_ = {};
		uint32_t slotCount_ = 0;
		uint32_t freeCount_ = 0;
		uint32_t objectCount_ = 0;
	};

	static_assert( sizeof( Handle<class SlotMapTestTag> ) == sizeof( uint64_t ) );
}

