#include "TestTemplate.hpp"

#include <memory>
#include <utility>

namespace ldx12::tests
{
	void TestSlotMapVirtualObjects()
	{
		using VirtualPointer = std::unique_ptr<VirtualObject>;
		SlotMap<VirtualPointer, 3> objects;
		uint32_t destructionCount = 0;

		VirtualPointer firstObject = std::make_unique<DerivedVirtualObject>( 7u, 3.5f, destructionCount );
		const Handle<VirtualPointer> first = objects.Create( std::move( firstObject ) );
		Require( firstObject == nullptr, "SlotMap did not take ownership of a virtual object." );
		VirtualPointer* storedObject = objects.Get( first );
		Require( storedObject != nullptr && *storedObject != nullptr, "SlotMap lost the virtual object." );
		Require( ( *storedObject )->Id() == 7u && ( *storedObject )->Property() == 3.5f, "Virtual dispatch or stored properties are incorrect." );

		objects.Destroy( first );
		Require( destructionCount == 1u, "Destroy did not release the owned virtual object." );

		objects.Create( std::make_unique<DerivedVirtualObject>( 8u, 4.0f, destructionCount ) );
		objects.Create( std::make_unique<DerivedVirtualObject>( 9u, 4.5f, destructionCount ) );
		objects.Clear();
		Require( destructionCount == 3u, "Clear did not release every owned virtual object." );
	}
}
