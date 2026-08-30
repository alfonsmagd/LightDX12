#include "TestTemplate.hpp"

namespace ldx12::tests
{
	void TestSlotMapCapacity()
	{
		SlotMap<TestObject, 2> objects;
		objects.Create( TestObject{ 1u, 1.0f } );
		objects.Create( TestObject{ 2u, 2.0f } );
		RequireThrows<std::length_error>( [ &objects ] { objects.Create( TestObject{ 3u, 3.0f } ); },
			"SlotMap accepted more objects than its fixed capacity." );
	}
}
