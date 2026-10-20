#include "Ldx12/TestTemplate.hpp"

#include <exception>
#include <iostream>
#include <string_view>

using namespace ldx12::tests;

int main( int argc, char** argv )
{
	const TestCase tests[] = { TestCase{ "CPU SlotMap create, retrieve, destroy and reuse", TestSlotMapLifecycle },
		TestCase{ "CPU SlotMap capacity, copy and move", TestSlotMapStorage },
		TestCase{ "CPU SlotMap static fragmentation and reuse", TestSlotMapFragmentation },
		TestCase{ "CPU SlotMap virtual objects", TestSlotMapVirtualObjects },
		TestCase{ "GPU adapter selection and fallback", TestGpuAdapterSelection },
		TestCase{ "GPU buffer and texture upload, readback and lifetime", TestGpuResourceLifecycle },
		TestCase{ "GPU compute UAV and CPU readback", TestGpuComputeUav },
		TestCase{ "GPU texture transitions across a batch and discard", TestGpuSubmissionSynchronization },
		TestCase{ "GPU descriptor recycling", TestGpuDescriptorRecycling },
		TestCase{ "GPU multisample rendering", TestGpuMultisampleRendering },
		TestCase{ "GPU array/cube sampling and deferred sampler release", TestGpuTextureArrayAndCubeViews } };

	if( argc > 2 )
	{
		std::cerr << "Usage: Ldx12Tests [--list | name-substring]\n";
		return 1;
	}

	const std::string_view filter = argc == 2 ? argv[ 1 ] : "";

	if( filter == "--list" )
	{
		for( const TestCase& test : tests )
		{
			std::cout << test.name << '\n';
		}

		return 0;
	}

	uint32_t passedCount = 0;
	uint32_t failedCount = 0;

	for( const TestCase& test : tests )
	{
		const std::string_view name = test.name;
		bool selected = name.find( filter ) != std::string_view::npos;

		if( filter == "CPU" || filter == "GPU" )
		{
			selected = name.starts_with( filter );
		}

		if( !selected )
		{
			continue;
		}

		try
		{
			test.function();
			++passedCount;
			std::cout << "[PASS] " << test.name << '\n';
		}
		catch( const std::exception& exception )
		{
			++failedCount;
			std::cerr << "[FAIL] " << test.name << ": " << exception.what() << '\n';
		}
		catch( ... )
		{
			++failedCount;
			std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
		}
	}

	if( passedCount + failedCount == 0 )
	{
		std::cerr << "No test matches '" << filter << "'. Use --list to see available cases.\n";
		return 1;
	}

	std::cout << "Ldx12 core tests: " << passedCount << " passed, " << failedCount << " failed.\n";
	return failedCount == 0 ? 0 : 1;
}
