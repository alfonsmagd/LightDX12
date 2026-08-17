#include "AudioManager2D.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <utility>

namespace frontier
{
	struct AudioManager2D::Impl final
	{
		static constexpr uint32_t kSampleRate = 48000;
		static constexpr size_t kGunshotFrameCount = kSampleRate * 380u / 1000u;
		static constexpr size_t kVoiceCount = 24;

		struct Voice final
		{
			ma_audio_buffer buffer{};
			ma_sound sound{};
			bool bufferInitialized = false;
			bool soundInitialized = false;
		};

		bool Initialize()
		{
			ma_engine_config engineConfig = ma_engine_config_init();
			engineConfig.sampleRate = kSampleRate;
			if( ma_engine_init( &engineConfig, &engine ) != MA_SUCCESS ) return false;
			engineInitialized = true;

			GenerateGunshot();
			for( Voice& voice : voices )
			{
				ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
					ma_format_f32, 1u, gunshotSamples.size(), gunshotSamples.data(), nullptr );
				bufferConfig.sampleRate = kSampleRate;
				if( ma_audio_buffer_init( &bufferConfig, &voice.buffer ) != MA_SUCCESS )
				{
					Shutdown();
					return false;
				}
				voice.bufferInitialized = true;
				if( ma_sound_init_from_data_source( &engine,
					reinterpret_cast<ma_data_source*>( &voice.buffer ),
					MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &voice.sound ) != MA_SUCCESS )
				{
					Shutdown();
					return false;
				}
				voice.soundInitialized = true;
			}

			ready = true;
			return true;
		}

		void Shutdown() noexcept
		{
			ready = false;
			for( Voice& voice : voices )
			{
				if( voice.soundInitialized )
				{
					ma_sound_uninit( &voice.sound );
					voice.soundInitialized = false;
				}
				if( voice.bufferInitialized )
				{
					ma_audio_buffer_uninit( &voice.buffer );
					voice.bufferInitialized = false;
				}
			}
			if( engineInitialized )
			{
				ma_engine_uninit( &engine );
				engineInitialized = false;
			}
			nextVoice = 0;
		}

		void PlayGunshot( const PlayDesc& desc ) noexcept
		{
			if( !ready ) return;

			Voice* selectedVoice = nullptr;
			for( size_t offset = 0; offset < voices.size(); ++offset )
			{
				Voice& candidate = voices[( nextVoice + offset ) % voices.size()];
				if( ma_sound_is_playing( &candidate.sound ) == MA_FALSE )
				{
					selectedVoice = &candidate;
					nextVoice = ( nextVoice + offset + 1u ) % voices.size();
					break;
				}
			}

			if( selectedVoice == nullptr )
			{
				selectedVoice = &voices[nextVoice];
				nextVoice = ( nextVoice + 1u ) % voices.size();
				ma_sound_stop( &selectedVoice->sound );
			}

			ma_sound_seek_to_pcm_frame( &selectedVoice->sound, 0u );
			ma_sound_reset_stop_time_and_fade( &selectedVoice->sound );
			ma_sound_set_volume( &selectedVoice->sound, std::clamp( desc.volume, 0.0f, 2.0f ) );
			ma_sound_set_pan( &selectedVoice->sound, std::clamp( desc.pan, -1.0f, 1.0f ) );
			ma_sound_set_pitch( &selectedVoice->sound,
				std::clamp( desc.pitch * RandomRange( 0.94f, 1.06f ), 0.25f, 4.0f ) );
			ma_sound_start( &selectedVoice->sound );
		}

		void GenerateGunshot() noexcept
		{
			uint32_t noiseState = 0x91e10da5u;
			float lowNoise = 0.0f;
			constexpr float twoPi = std::numbers::pi_v<float> * 2.0f;
			constexpr size_t echoDelay = kSampleRate * 47u / 1000u;

			for( size_t frame = 0; frame < gunshotSamples.size(); ++frame )
			{
				noiseState = noiseState * 1664525u + 1013904223u;
				const float noise = static_cast<float>( ( noiseState >> 8u ) & 0x00ffffffu ) /
					static_cast<float>( 0x007fffffu ) - 1.0f;
				lowNoise += ( noise - lowNoise ) * 0.075f;
				const float time = static_cast<float>( frame ) / static_cast<float>( kSampleRate );
				const float crack = noise * std::exp( -time * 72.0f ) * 1.25f;
				const float body = lowNoise * std::exp( -time * 12.5f ) * 2.4f;
				const float boomPhase = twoPi * ( 105.0f * time - 74.0f * time * time );
				const float boom = std::sin( boomPhase ) * std::exp( -time * 11.0f ) * 0.78f;
				const float snap = std::sin( twoPi * 1850.0f * time ) * std::exp( -time * 95.0f ) * 0.26f;
				float sample = crack + body + boom + snap;
				if( frame >= echoDelay ) sample += gunshotSamples[frame - echoDelay] * 0.16f;
				gunshotSamples[frame] = std::tanh( sample * 1.35f ) * 0.82f;
			}
		}

		float RandomRange( float minimum, float maximum ) noexcept
		{
			randomState = randomState * 1664525u + 1013904223u;
			const float normalized = static_cast<float>( ( randomState >> 8u ) & 0x00ffffffu ) /
				static_cast<float>( 0x00ffffffu );
			return minimum + ( maximum - minimum ) * normalized;
		}

		ma_engine engine{};
		std::array<float, kGunshotFrameCount> gunshotSamples{};
		std::array<Voice, kVoiceCount> voices{};
		size_t nextVoice = 0;
		uint32_t randomState = 0x4d595df4u;
		bool engineInitialized = false;
		bool ready = false;
	};

	AudioManager2D& AudioManager2D::Get() noexcept
	{
		static AudioManager2D manager;
		return manager;
	}

	AudioManager2D::~AudioManager2D()
	{
		Shutdown();
	}

	bool AudioManager2D::Initialize()
	{
		if( IsInitialized() ) return true;
		std::unique_ptr<Impl> implementation = std::make_unique<Impl>();
		if( !implementation->Initialize() ) return false;
		impl_ = std::move( implementation );
		return true;
	}

	void AudioManager2D::Shutdown() noexcept
	{
		if( impl_ == nullptr ) return;
		impl_->Shutdown();
		impl_.reset();
	}

	bool AudioManager2D::IsInitialized() const noexcept
	{
		return impl_ != nullptr && impl_->ready;
	}

	void AudioManager2D::SetMasterVolume( float volume ) noexcept
	{
		if( !IsInitialized() ) return;
		ma_engine_set_volume( &impl_->engine, std::clamp( volume, 0.0f, 2.0f ) );
	}

	void AudioManager2D::Play( Sound sound, const PlayDesc& desc ) noexcept
	{
		if( !IsInitialized() ) return;
		switch( sound )
		{
			case Sound::RevolverShot:
				impl_->PlayGunshot( desc );
				break;
		}
	}
}
