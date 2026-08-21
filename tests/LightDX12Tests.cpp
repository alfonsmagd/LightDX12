#include "LightDX12/TestTemplate.hpp"

#include <array>
#include <exception>
#include <iostream>

using namespace lightd3d12::tests;

int main()
{
	const std::array<TestCase, 11> tests = {
		TestCase{ "SlotMap creation and properties", TestSlotMapCreationAndProperties },
		TestCase{ "SlotMap destruction and reuse", TestSlotMapDestroyAndReuse },
		TestCase{ "SlotMap stale-handle safety", TestSlotMapStaleHandleSafety },
		TestCase{ "SlotMap fixed capacity", TestSlotMapCapacity },
		TestCase{ "SlotMap virtual objects", TestSlotMapVirtualObjects },
		TestCase{ "Public fixed-array properties", TestPublicArrayProperties },
		TestCase{ "GPU resource lifecycle and properties", TestGpuResourceLifecycleAndProperties },
		TestCase{ "GPU invalid-handle safety", TestGpuInvalidHandleSafety },
		TestCase{ "GPU submission synchronization", TestGpuSubmissionSynchronization },
		TestCase{ "GPU batch submission", TestGpuBatchSubmission },
		TestCase{ "GPU descriptor recycling", TestGpuDescriptorRecycling }
	};

	uint32_t passedCount = 0;
	for( const TestCase& test : tests )
	{
		try
		{
			test.function();
			++passedCount;
			std::cout << "[PASS] " << test.name << '\n';
		}
		catch( const std::exception& exception )
		{
			std::cerr << "[FAIL] " << test.name << ": " << exception.what() << '\n';
			return 1;
		}
		catch( ... )
		{
			std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
			return 1;
		}
	}

	std::cout << "LightDX12 core tests passed: " << passedCount << '/' << tests.size() << ".\n";
	return 0;
}
