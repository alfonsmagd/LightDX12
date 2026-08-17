#include "FrontierApplication.hpp"
#include "FrontierRun.hpp"
#include "App/WindowApplication.hpp"

#include <cwchar>
#include <stdexcept>

using namespace lightd3d12;

namespace
{
	class FrontierApplication final : public App::IWindowApplication
	{
	public:
		explicit FrontierApplication( bool smokeTest ) noexcept
			: smokeTest_( smokeTest )
		{
		}

		void Initialize( RenderDevice& device, DXGI_FORMAT colorFormat ) override
		{
			run_.Initialize( device, colorFormat );
		}

		void Physics( float deltaSeconds ) override
		{
			const bool restartDown = App::IsKeyDown( 'R' );
			if( restartDown && !restartWasDown_ ) run_.Reset();
			restartWasDown_ = restartDown;
			run_.Physics( deltaSeconds, ReadInput() );
		}

		void Render( ICommandBuffer& commands ) override
		{
			run_.Render( commands );
		}

		void Shutdown() override
		{
			run_.Shutdown();
		}

		bool ShouldClose() const noexcept override
		{
			return smokeTest_ && run_.World().animationTime >= 0.25f;
		}

	private:
		static frontier::InputState ReadInput()
		{
			frontier::InputState input;
			if( App::IsKeyDown( 'A' ) ) input.movement.x -= 1.0f;
			if( App::IsKeyDown( 'D' ) ) input.movement.x += 1.0f;
			if( App::IsKeyDown( 'W' ) ) input.movement.y -= 1.0f;
			if( App::IsKeyDown( 'S' ) ) input.movement.y += 1.0f;
			if( App::IsKeyDown( VK_LEFT ) ) input.fireDirection.x -= 1.0f;
			if( App::IsKeyDown( VK_RIGHT ) ) input.fireDirection.x += 1.0f;
			if( App::IsKeyDown( VK_UP ) ) input.fireDirection.y -= 1.0f;
			if( App::IsKeyDown( VK_DOWN ) ) input.fireDirection.y += 1.0f;
			input.fireUsingLastDirection = App::IsKeyDown( VK_SPACE );
			return input;
		}

		frontier::FrontierRun run_;
		bool smokeTest_ = false;
		bool restartWasDown_ = false;
	};
}

int frontier::RunApplication( HINSTANCE instance, PWSTR commandLine, int showCommand )
{
	const bool smokeTest = commandLine != nullptr && std::wcsstr( commandLine, L"--smoke-test" ) != nullptr;
	FrontierApplication application( smokeTest );
	App::WindowApplicationDesc desc;
	desc.className = L"LightDX12FrontierRiders2D";
	desc.title = L"LightDX12 - Frontier Riders 2D";
	desc.debugLabel = "Frontier Riders 2D";

	try
	{
		return App::RunWindowApplication( instance, showCommand, desc, application );
	}
	catch( const std::exception& error )
	{
		MessageBoxA( nullptr, error.what(), "Frontier Riders 2D", MB_OK | MB_ICONERROR );
		return 1;
	}
}
