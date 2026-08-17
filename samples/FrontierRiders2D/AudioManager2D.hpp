#pragma once

#include <cstdint>
#include <memory>

namespace frontier
{
	class AudioManager2D final
	{
	public:
		enum class Sound : uint8_t
		{
			RevolverShot,
		};

		struct PlayDesc
		{
			float volume = 1.0f;
			float pan = 0.0f;
			float pitch = 1.0f;
		};

		static AudioManager2D& Get() noexcept;

		AudioManager2D( const AudioManager2D& ) = delete;
		AudioManager2D& operator=( const AudioManager2D& ) = delete;
		AudioManager2D( AudioManager2D&& ) = delete;
		AudioManager2D& operator=( AudioManager2D&& ) = delete;
		~AudioManager2D();

		bool Initialize();
		void Shutdown() noexcept;
		bool IsInitialized() const noexcept;

		void SetMasterVolume( float volume ) noexcept;
		void Play( Sound sound, const PlayDesc& desc = {} ) noexcept;

	private:
		AudioManager2D() = default;

		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
