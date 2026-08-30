#include "TestTemplate.hpp"

namespace ldx12::tests
{
	void TestSlotMapStaleHandleSafety()
	{
		SlotMap<TestObject, 2> objects;
		Require( objects.Empty(), "A new SlotMap is not empty." );
		Require( objects.MaxSize() == 2u, "SlotMap does not expose its fixed capacity." );
		Require( !objects.Contains( Handle<TestObject>{} ), "SlotMap accepted an empty handle." );
		Require( !objects.Destroy( Handle<TestObject>{} ), "Destroy reported success for an empty handle." );

		const Handle<TestObject> removed = objects.Create( TestObject{ 7u, 1.0f } );
		Require( objects.Contains( removed ), "SlotMap does not contain a newly created handle." );
		Require( objects.Destroy( removed ), "Destroy failed for a live handle." );
		Require( objects.Empty(), "Destroy did not empty the SlotMap." );
		Require( !objects.Contains( removed ), "Destroyed handle remains alive." );
		Require( objects.Get( removed ) == nullptr, "Destroyed handle still resolves to an object." );
		Require( !objects.Destroy( removed ), "Double destruction reported success." );
		Require( objects.Size() == 0u, "Double destruction corrupted the object count." );

		const Handle<TestObject> replacement = objects.Create( TestObject{ 9u, 2.0f } );
		Require( replacement.Index() == removed.Index(), "Released slot was not recycled." );
		Require( replacement.Gen() != removed.Gen(), "Recycled slot retained its old generation." );
		Require( !objects.Contains( removed ), "Stale handle aliases the replacement object." );
		Require( objects.Get( removed ) == nullptr, "Stale handle retrieved the replacement object." );
		Require( !objects.Destroy( removed ), "Stale handle destroyed the replacement object." );
		Require( objects.Contains( replacement ), "Stale destruction invalidated the replacement handle." );
		Require( objects.Get( replacement )->id == 9u, "Replacement object was modified by a stale handle." );
		Require( objects.Destroy( replacement ), "Destroy failed for the replacement object." );
		Require( objects.Empty(), "SlotMap is not empty after destroying every object." );
	}
}
