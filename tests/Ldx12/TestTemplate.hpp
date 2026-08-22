#pragma once

#include "Ldx12/HandleSlotMap.hpp"
#include "Ldx12/Ldx12.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>

namespace ldx12::tests
{
	inline void Require( bool condition, const char* message )
	{
		if( !condition )
		{
			throw std::runtime_error( message );
		}
	}

	template<typename ExceptionType, typename Function>
	void RequireThrows( Function&& function, const char* message )
	{
		try
		{
			function();
		}
		catch( const ExceptionType& )
		{
			return;
		}

		throw std::runtime_error( message );
	}

	struct TestObject final
	{
		uint32_t id = 0;
		float weight = 0.0f;
	};

	class VirtualObject
	{
	public:
		virtual ~VirtualObject() = default;
		virtual uint32_t Id() const noexcept = 0;
		virtual float Property() const noexcept = 0;
	};

	class DerivedVirtualObject final: public VirtualObject
	{
	public:
		DerivedVirtualObject(
			uint32_t id,
			float property,
			uint32_t& destructionCount ) noexcept:
			id_( id ),
			property_( property ),
			destructionCount_( &destructionCount )
		{
		}

		~DerivedVirtualObject() override
		{
			++(*destructionCount_);
		}

		uint32_t Id() const noexcept override { return id_; }
		float Property() const noexcept override { return property_; }

	private:
		uint32_t id_ = 0;
		float property_ = 0.0f;
		uint32_t* destructionCount_ = nullptr;
	};

	struct DeviceManagerGuard final
	{
		~DeviceManagerGuard()
		{
			if( active )
			{
				DeviceManager::ShutdownSingleton();
			}
		}

		bool active = false;
	};

	struct TestCase final
	{
		const char* name = nullptr;
		void (*function)() = nullptr;
	};

	void TestSlotMapCreationAndProperties();
	void TestSlotMapDestroyAndReuse();
	void TestSlotMapStaleHandleSafety();
	void TestSlotMapCapacity();
	void TestSlotMapVirtualObjects();
	void TestPublicArrayProperties();
	void TestGpuResourceLifecycleAndProperties();
	void TestGpuInvalidHandleSafety();
	void TestGpuSubmissionSynchronization();
	void TestGpuDescriptorRecycling();
}
