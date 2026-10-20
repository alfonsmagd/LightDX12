#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "Ldx12/Ldx12_Defines.hpp"

namespace ldx12
{
	template <typename ResourceType> class Handle final
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
			return !( *this == other );
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
		Handle( uint32_t index, uint32_t gen ) noexcept : index_( index ), gen_( gen )
		{
		}

		template <typename T, std::size_t Capacity> friend class SlotMap;
		friend class DeviceManager;

		uint32_t index_ = 0;
		uint32_t gen_ = 0;
	};

	template <typename ObjectType, std::size_t Capacity> class SlotMap final
	{
		static_assert( Capacity > 0 );
		static_assert( Capacity <= std::numeric_limits<uint32_t>::max() );
		static_assert( Capacity <= LDX12_MAX_RESOURCES, "Slot capacity exceeds the selected resource profile." );

		struct Slot
		{
			ObjectType object = {};
			uint32_t generation = 1;
			bool occupied = false;
		};

	public:
		SlotMap() = default;

		Handle<ObjectType> Create( ObjectType&& object )
		{
			const bool reuseFreeSlot = freeSlotCount_ > 0;
			uint32_t slotIndex = slotCount_;

			if( reuseFreeSlot )
			{
				slotIndex = freeSlotIndices_[ freeSlotCount_ - 1 ];
			}
			else if( slotCount_ == Capacity )
			{
				throw std::length_error( "Ldx12 SlotMap capacity exhausted." );
			}

			Slot& slot = slots_[ slotIndex ];

			slot.object = std::move( object );
			slot.occupied = true;

			if( reuseFreeSlot )
			{
				--freeSlotCount_;
			}
			else
			{
				++slotCount_;
			}

			++objectCount_;
			return Handle<ObjectType>( slotIndex, slot.generation );
		}

		bool Contains( Handle<ObjectType> handle ) const noexcept
		{
			if( !handle.Valid() || handle.Index() >= slotCount_ )
			{
				return false;
			}

			const Slot& slot = slots_[ handle.Index() ];

			return slot.occupied && handle.Gen() == slot.generation;
		}

		bool Destroy( Handle<ObjectType> handle )
		{
			if( !Contains( handle ) )
			{
				return false;
			}

			const uint32_t slotIndex = handle.Index();
			Slot& slot = slots_[ slotIndex ];

			slot.object = {};
			IncrementGeneration( slot );
			slot.occupied = false;

			freeSlotIndices_[ freeSlotCount_ ] = slotIndex;
			++freeSlotCount_;
			--objectCount_;
			return true;
		}

		ObjectType* Get( Handle<ObjectType> handle )
		{
			return Contains( handle ) ? &slots_[ handle.Index() ].object : nullptr;
		}

		const ObjectType* Get( Handle<ObjectType> handle ) const
		{
			return Contains( handle ) ? &slots_[ handle.Index() ].object : nullptr;
		}

		ObjectType* GetByIndex( uint32_t slotIndex )
		{
			if( slotIndex >= slotCount_ )
			{
				return nullptr;
			}

			Slot& slot = slots_[ slotIndex ];

			return slot.occupied ? &slot.object : nullptr;
		}

		const ObjectType* GetByIndex( uint32_t slotIndex ) const
		{
			if( slotIndex >= slotCount_ )
			{
				return nullptr;
			}

			const Slot& slot = slots_[ slotIndex ];

			return slot.occupied ? &slot.object : nullptr;
		}

		Handle<ObjectType> Find( const ObjectType* object ) const
		{
			if( object == nullptr )
			{
				return {};
			}

			for( uint32_t slotIndex = 0; slotIndex < slotCount_; ++slotIndex )
			{
				const Slot& slot = slots_[ slotIndex ];

				if( slot.occupied && &slot.object == object )
				{
					return Handle<ObjectType>( slotIndex, slot.generation );
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
			// Preserve slot generations so handles from before Clear() stay invalid.
			for( uint32_t slotIndex = 0; slotIndex < slotCount_; ++slotIndex )
			{
				Slot& slot = slots_[ slotIndex ];

				if( slot.occupied )
				{
					slot.object = {};
					IncrementGeneration( slot );
					slot.occupied = false;
				}
			}

			slotCount_ = 0;
			freeSlotCount_ = 0;
			objectCount_ = 0;
		}

		template <typename Function> void ForEach( Function&& function )
		{
			for( uint32_t slotIndex = 0; slotIndex < slotCount_; ++slotIndex )
			{
				Slot& slot = slots_[ slotIndex ];

				if( slot.occupied )
				{
					std::forward<Function>( function )( slot.object );
				}
			}
		}

		template <typename Function> void ForEach( Function&& function ) const
		{
			for( uint32_t slotIndex = 0; slotIndex < slotCount_; ++slotIndex )
			{
				const Slot& slot = slots_[ slotIndex ];

				if( slot.occupied )
				{
					std::forward<Function>( function )( slot.object );
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

		SlotMap( const SlotMap& other )
			requires std::is_copy_constructible_v<ObjectType>
		= default;

		SlotMap& operator=( const SlotMap& other )
			requires std::is_copy_constructible_v<ObjectType>
		= default;

		SlotMap( SlotMap&& other ) noexcept( std::is_nothrow_move_assignable_v<ObjectType> )
		{
			*this = std::move( other );
		}

		SlotMap& operator=( SlotMap&& other ) noexcept( std::is_nothrow_move_assignable_v<ObjectType> )
		{
			if( this != &other )
			{
				slots_ = std::move( other.slots_ );
				freeSlotIndices_ = std::move( other.freeSlotIndices_ );
				slotCount_ = other.slotCount_;
				freeSlotCount_ = other.freeSlotCount_;
				objectCount_ = other.objectCount_;

				other.Clear();
			}

			return *this;
		}

	private:
		static void IncrementGeneration( Slot& slot ) noexcept
		{
			++slot.generation;
			if( slot.generation == 0 )
			{
				slot.generation = 1;
			}
		}

		std::array<Slot, Capacity> slots_ = {};
		std::array<uint32_t, Capacity> freeSlotIndices_ = {};
		// Slots reached so far, including released slots.
		uint32_t slotCount_ = 0;
		uint32_t freeSlotCount_ = 0;
		uint32_t objectCount_ = 0;
	};

	static_assert( sizeof( Handle<class SlotMapTestTag> ) == sizeof( uint64_t ) );
}
