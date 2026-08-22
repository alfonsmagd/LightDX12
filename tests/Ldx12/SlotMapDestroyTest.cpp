#include "TestTemplate.hpp"

namespace ldx12::tests
{
	void TestSlotMapDestroyAndReuse()
	{
		SlotMap<TestObject, 3> objects;
		const Handle<TestObject> first = objects.Create( TestObject{ 1u, 1.0f } );
		const Handle<TestObject> removed = objects.Create( TestObject{ 2u, 2.0f } );
		objects.Create( TestObject{ 3u, 3.0f } );

		objects.Destroy( removed );
		Require( objects.Size() == 2u, "SlotMap destruction did not update the count." );
		Require( objects.GetByIndex( removed.Index() ) == nullptr,
			"Destroyed SlotMap storage remains occupied." );

		const Handle<TestObject> replacement =
			objects.Create( TestObject{ 42u, 4.2f } );
		Require( replacement.Index() == removed.Index(),
			"SlotMap did not reuse a released slot." );
		Require( replacement.Gen() != removed.Gen(),
			"Reused SlotMap slot did not change generation." );
		Require( replacement != removed,
			"A stale handle compares equal to its replacement." );
		Require( objects.Get( replacement )->id == 42u,
			"Replacement object properties are incorrect." );

		objects.Clear();
		Require( objects.Size() == 0u, "SlotMap Clear did not remove every object." );
		const Handle<TestObject> afterClear =
			objects.Create( TestObject{ 99u, 9.9f } );
		Require( afterClear.Index() == 0u,
			"SlotMap did not restart at slot zero after Clear." );
		Require( afterClear != first,
			"SlotMap Clear did not invalidate old generations." );
	}
}
