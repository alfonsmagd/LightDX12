#include "TestTemplate.hpp"

namespace ldx12::tests
{
	void TestSlotMapLifecycle()
	{
		SlotMap<TestObject, 2> objects;

		Require( objects.Empty() && !objects.Destroy( Handle<TestObject>{} ), "An empty SlotMap accepted an empty handle." );

		const Handle<TestObject> first = objects.Create( TestObject{ 10u, 1.5f } );
		const Handle<TestObject> second = objects.Create( TestObject{ 20u, 2.5f } );
		const TestObject* firstObject = objects.Get( first );

		Require( firstObject != nullptr && objects.Get( second ) != nullptr, "A created object cannot be retrieved." );
		Require( firstObject->id == 10u && firstObject->weight == 1.5f, "Creation changed the stored properties." );
		Require( objects.GetByIndex( second.Index() )->id == 20u && objects.Find( firstObject ) == first, "Object lookup returned the wrong resource." );

		uint32_t visitedCount = 0;
		uint32_t idSum = 0;

		objects.ForEach(
			[ & ]( const TestObject& object )
			{
				++visitedCount;
				idSum += object.id;
			} );
		Require( visitedCount == 2u && idSum == 30u, "Iteration did not visit the two live objects." );

		Require( objects.Destroy( second ), "Destroy failed for a live handle." );
		Require( objects.Size() == 1u && objects.GetByIndex( second.Index() ) == nullptr, "Destroy left the slot occupied." );
		Require( !objects.Destroy( second ), "Double destruction succeeded." );

		const Handle<TestObject> replacement = objects.Create( TestObject{ 42u, 4.2f } );

		Require( replacement.Index() == second.Index() && replacement.Gen() != second.Gen(), "Reuse did not change the released slot's generation." );
		Require( !objects.Contains( second ) && objects.Get( second ) == nullptr, "The stale handle resolves to its replacement." );
		Require( !objects.Destroy( second ) && objects.Contains( replacement ), "Stale destruction removed the replacement." );
		Require( objects.Get( replacement )->id == 42u, "Slot reuse lost the replacement's properties." );

		// Clear a survivor and a hole, then refill both slots without reviving either generation.
		objects.Destroy( first );
		objects.Clear();

		Require( objects.Empty(), "Clear left live objects behind." );

		for( uint32_t index = 0; index < objects.MaxSize(); ++index )
		{
			const Handle<TestObject> refilled = objects.Create( TestObject{ index, 1.0f } );

			Require( refilled.Index() == index, "Clear did not restore sequential allocation." );
		}

		Require( !objects.Contains( first ) && !objects.Contains( second ) && !objects.Contains( replacement ),
			"Refilling after Clear revived an old handle." );
	}
}
