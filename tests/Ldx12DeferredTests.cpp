#include "Ldx12Internal.hpp"

#include <iostream>
#include <stdexcept>

namespace ldx12
{
	// Narrow access to the actual private guard; no device or GPU is created.
	struct DeferredReleaseTestAccess
	{
		static void Run()
		{
			using Deferred = DeviceManager::DeferredRelease;
			int cleanupCount = 0;
			{
				Deferred::OnFailure cleanup( [ &cleanupCount ]() noexcept { ++cleanupCount; } );
			}
			if( cleanupCount != 0 )
				throw std::runtime_error( "Normal exit ran cleanup." );
			std::cout << "[PASS] Normal exit retains the resource\n";

			struct ExpectedFailure{};
			bool propagated = false;
			try
			{
				Deferred::OnFailure cleanup( [ &cleanupCount ]() noexcept { ++cleanupCount; } );
				throw ExpectedFailure{};
			}
			catch( const ExpectedFailure& )
			{
				propagated = true;
				if( cleanupCount != 1 )
					throw std::runtime_error( "Cleanup did not run exactly once before the handler." );
			}
			if( !propagated )
				throw std::runtime_error( "The original exception was swallowed." );
			std::cout << "[PASS] Exception runs cleanup once and continues to the caller\n";

			{
				Deferred::OnFailure cleanup( [ &cleanupCount ]() noexcept { ++cleanupCount; } );
				try
				{
					throw ExpectedFailure{};
				}
				catch( const ExpectedFailure& )
				{
					std::cout << "[PASS] incremented ++ cleanupcountg \n";
				}
			}
			if( cleanupCount != 1 )
				throw std::runtime_error( "A locally handled exception ran cleanup." );
			std::cout << "[PASS] Locally handled exception retains the resource\n";
		}
	};
}

int main()
{
	try
	{
		ldx12::DeferredReleaseTestAccess::Run();
		return 0;
	}
	catch( const std::exception& error )
	{
		std::cerr << "[FAIL] " << error.what() << '\n';
		return 1;
	}
}
