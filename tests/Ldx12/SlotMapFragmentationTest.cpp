#include "TestTemplate.hpp"

#include <array>

namespace ldx12::tests
{
	void TestSlotMapFragmentation()
	{
		constexpr uint32_t slotsPerRegion = 64;
		constexpr uint32_t resourceCount = slotsPerRegion * 3;
		constexpr uint32_t firstRegionReleaseCount = 53;
		constexpr uint32_t secondRegionReleaseCount = 32;
		constexpr uint32_t firstReleaseCount = firstRegionReleaseCount + secondRegionReleaseCount;
		constexpr uint32_t cycleReleaseCount = slotsPerRegion + 32;

		SlotMap<TestObject, resourceCount> objects;
		std::array<Handle<TestObject>, resourceCount> handles{};
		std::array<TestObject*, resourceCount> originalAddresses{};
		std::array<uint32_t, resourceCount> releaseOrder{};
		std::array<Handle<TestObject>, resourceCount> staleHandles{};

		for( uint32_t index = 0; index < resourceCount; ++index )
		{
			handles[ index ] = objects.Create( TestObject{ index, 1.0f } );
			originalAddresses[ index ] = objects.Get( handles[ index ] );
		}

		// Three 64-slot regions keep cross-region reuse visible in the 32/53-hole regression.
		const auto verifyLiveObjects = [ & ]()
		{
			uint32_t liveCount = 0;

			for( uint32_t index = 0; index < resourceCount; ++index )
			{
				if( handles[ index ].Empty() )
				{
					Require( objects.GetByIndex( index ) == nullptr, "A fragmented slot still exposes an object." );
					continue;
				}

				const TestObject* object = objects.Get( handles[ index ] );

				Require( object == originalAddresses[ index ], "Fragmentation or reuse moved an object to a different address." );
				Require( objects.GetByIndex( index ) == object, "Indexed access disagrees with handle access after fragmentation." );
				Require( object->id == index && object->weight == 1.0f, "Fragmentation or reuse changed object properties." );
				++liveCount;
			}

			std::array<bool, resourceCount> visited{};
			uint32_t visitedCount = 0;

			objects.ForEach(
				[ & ]( const TestObject& object )
				{
					Require( object.id < resourceCount, "Iteration exposed an invalid object after fragmentation." );
					Require( handles[ object.id ].Valid() && !visited[ object.id ], "Iteration exposed a hole or visited an object twice." );
					visited[ object.id ] = true;
					++visitedCount;
				} );

			Require( objects.Size() == liveCount && visitedCount == liveCount, "Fragmentation corrupted the live object count or iteration." );
		};

		// Release 32 scattered slots in region 1, then 53 scattered slots in region 0.
		for( uint32_t index = 0; index < secondRegionReleaseCount; ++index )
		{
			releaseOrder[ index ] = slotsPerRegion + index * 2;
		}

		for( uint32_t index = 0; index < firstRegionReleaseCount; ++index )
		{
			releaseOrder[ secondRegionReleaseCount + index ] = ( index * 17 ) % slotsPerRegion;
		}

		for( uint32_t index = 0; index < firstReleaseCount; ++index )
		{
			const uint32_t slotIndex = releaseOrder[ index ];
			staleHandles[ index ] = handles[ slotIndex ];

			Require( objects.Destroy( handles[ slotIndex ] ), "Could not create a hole in the requested region." );
			handles[ slotIndex ] = {};
			Require( objects.Get( staleHandles[ index ] ) == nullptr, "A released slot still accepts its old handle." );
		}

		Require( objects.Size() == resourceCount - firstReleaseCount, "Releasing 32 and 53 slots removed the wrong number of objects." );
		verifyLiveObjects();

		// The first 53 replacements go to region 0; the next 32 go to region 1.
		for( uint32_t index = 0; index < firstReleaseCount; ++index )
		{
			const uint32_t releaseIndex = firstReleaseCount - 1 - index;
			const uint32_t expectedSlot = releaseOrder[ releaseIndex ];
			const Handle<TestObject> replacement = objects.Create( TestObject{ expectedSlot, 1.0f } );

			Require( replacement.Index() == expectedSlot, "The 32/53 scenario did not reuse slots in reverse release order." );
			Require( replacement.Gen() != staleHandles[ releaseIndex ].Gen(), "Reusing a fragmented slot kept its old generation." );
			Require( !objects.Destroy( staleHandles[ releaseIndex ] ), "An old handle destroyed a replacement in a fragmented slot." );
			handles[ expectedSlot ] = replacement;

			if( index + 1 == firstRegionReleaseCount )
			{
				Require( objects.Size() == resourceCount - secondRegionReleaseCount, "Refilling region 0 unexpectedly consumed region 1 holes." );
				verifyLiveObjects();
			}
		}

		verifyLiveObjects();

		// Empty the middle region, then scatter holes across the outer regions for eight cycles.
		for( uint32_t cycle = 0; cycle < 8; ++cycle )
		{
			for( uint32_t index = 0; index < slotsPerRegion; ++index )
			{
				releaseOrder[ index ] = slotsPerRegion + index;
			}

			for( uint32_t index = 0; index < 16; ++index )
			{
				const uint32_t indexInRegion = ( cycle * 5 + index ) % slotsPerRegion;

				releaseOrder[ slotsPerRegion + index * 2 ] = indexInRegion;
				releaseOrder[ slotsPerRegion + index * 2 + 1 ] = slotsPerRegion * 2 + indexInRegion;
			}

			for( uint32_t index = 0; index < cycleReleaseCount; ++index )
			{
				const uint32_t slotIndex = releaseOrder[ index ];
				staleHandles[ index ] = handles[ slotIndex ];

				Require( objects.Destroy( handles[ slotIndex ] ), "Could not fragment a region during repeated release cycles." );
				handles[ slotIndex ] = {};
			}

			Require( objects.Size() == resourceCount - cycleReleaseCount, "Repeated fragmentation corrupted the live object count." );
			verifyLiveObjects();

			for( uint32_t index = 0; index < cycleReleaseCount; ++index )
			{
				const uint32_t releaseIndex = cycleReleaseCount - 1 - index;
				const uint32_t expectedSlot = releaseOrder[ releaseIndex ];
				const Handle<TestObject> replacement = objects.Create( TestObject{ expectedSlot, 1.0f } );

				Require( replacement.Index() == expectedSlot, "Repeated fragmentation lost a free slot or changed reuse order." );
				Require( replacement.Gen() != staleHandles[ releaseIndex ].Gen(), "Repeated reuse kept an old generation." );
				Require( objects.Get( staleHandles[ releaseIndex ] ) == nullptr, "Repeated reuse revived a stale handle." );
				handles[ expectedSlot ] = replacement;
			}

			Require( objects.Size() == resourceCount, "Refilling fragmented regions lost resource capacity." );
			verifyLiveObjects();
			RequireThrows<std::length_error>( [ &objects ] { objects.Create( TestObject{} ); },
				"Repeated fragmentation allowed allocation beyond the fixed capacity." );
		}
	}
}
