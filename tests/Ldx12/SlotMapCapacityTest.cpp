#include "TestTemplate.hpp"

#include <array>
#include <utility>

namespace ldx12::tests
{
	void TestSlotMapStorage()
	{
		SlotMap<TestObject, 4> original;
		std::array<Handle<TestObject>, 4> handles{};

		handles[ 0 ] = original.Create( TestObject{ 0u, 1.0f } );
		const TestObject* firstAddress = original.Get( handles[ 0 ] );

		for( uint32_t index = 1; index < handles.size(); ++index )
		{
			handles[ index ] = original.Create( TestObject{ index, 1.0f } );
			Require( original.Get( handles[ 0 ] ) == firstAddress, "Filling storage moved a live object." );
		}

		RequireThrows<std::length_error>( [ & ] { original.Create( TestObject{} ); }, "SlotMap accepted an object beyond capacity." );
		Require( original.Size() == original.MaxSize(), "Rejected creation changed the live count." );

		original.Destroy( handles[ 1 ] );
		original.Destroy( handles[ 3 ] );

		SlotMap<TestObject, 4> copied( original );
		SlotMap<TestObject, 4> assigned;

		assigned = original;

		// Copies own their objects and preserve the same reusable holes.
		for( SlotMap<TestObject, 4>* copy : { &copied, &assigned } )
		{
			TestObject* object = copy->Get( handles[ 0 ] );

			Require( object != nullptr && object != firstAddress, "Copying did not preserve independent object storage." );
			Require( object->id == 0u && object->weight == 1.0f && copy->Size() == 2u, "Copying changed the stored objects." );
			object->weight = 5.0f;

			const Handle<TestObject> replacement = copy->Create( TestObject{ 30u, 3.0f } );

			Require( replacement.Index() == 3u && !copy->Contains( handles[ 3 ] ), "Copying lost a free slot or its generation." );
		}

		SlotMap<TestObject, 4> moved( std::move( copied ) );

		Require( copied.Empty() && moved.Contains( handles[ 0 ] ), "Move construction did not transfer the live objects." );
		assigned = std::move( moved );
		Require( moved.Empty() && assigned.Contains( handles[ 0 ] ), "Move assignment did not transfer the live objects." );
		Require( assigned.Get( handles[ 0 ] )->weight == 5.0f, "Moving lost the stored properties." );

		const Handle<TestObject> movedReplacement = assigned.Create( TestObject{} );
		const Handle<TestObject> sourceReplacement = copied.Create( TestObject{} );

		Require( movedReplacement.Index() == 1u && !assigned.Contains( handles[ 1 ] ), "Moving lost the remaining hole or revived its old handle." );
		Require( sourceReplacement.Index() == 0u && sourceReplacement.Gen() != handles[ 0 ].Gen(), "Reusing the moved source revived its old handle." );
		Require( original.Size() == 2u && firstAddress->weight == 1.0f, "Editing the copies modified the original." );
	}
}
