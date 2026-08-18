#include "TestTemplate.hpp"

namespace lightd3d12::tests
{
	void TestSlotMapCreationAndProperties()
	{
		SlotMap<TestObject, 4> objects;
		const Handle<TestObject> first = objects.Create( TestObject{ 10u, 1.5f } );
		const Handle<TestObject> second = objects.Create( TestObject{ 20u, 2.5f } );

		Require( first.Valid() && second.Valid(), "SlotMap returned an invalid handle." );
		Require( first != second, "Different objects received the same handle." );
		Require( first.Index() == 0u && second.Index() == 1u,
			"SlotMap did not allocate sequential initial slots." );
		Require( first.Gen() == 1u && second.Gen() == 1u,
			"Initial SlotMap generations are incorrect." );
		Require( objects.Size() == 2u && objects.NumObjects() == 2u,
			"SlotMap object count is incorrect." );

		TestObject* firstObject = objects.Get( first );
		Require( firstObject != nullptr, "Cannot retrieve a live SlotMap object." );
		Require( firstObject->id == 10u && firstObject->weight == 1.5f,
			"SlotMap did not preserve object properties." );
		Require( objects.GetByIndex( second.Index() )->id == 20u,
			"SlotMap index lookup returned the wrong object." );
		Require( objects.Find( firstObject ) == first,
			"SlotMap reverse lookup returned the wrong handle." );

		uint32_t visitedCount = 0;
		uint32_t idSum = 0;
		objects.ForEach( [&visitedCount, &idSum]( const TestObject& object )
			{
				++visitedCount;
				idSum += object.id;
			} );
		Require( visitedCount == 2u && idSum == 30u,
			"SlotMap iteration did not visit every live object." );
	}
}
