#pragma once
#include <cstdint>
namespace desktop_retro
{
	struct RetroPushConstants
	{
		uint32_t sourceTextureIndex = 0;
		uint32_t mode = 3;
		float intensity = 1.00f;
		float curvature = 0.032f;
		float scanlineStrength = 0.48f;
		float chromaticAberrationPixels = 0.65f;
		float noiseStrength = 0.0f;
		float time = 0.0f;
		float inverseWidth = 1.0f;
		float inverseHeight = 1.0f;
		uint32_t blurTextureIndex = 0;
		uint32_t reserved = 0;
	};

	static_assert( sizeof( RetroPushConstants ) / sizeof( uint32_t ) <= 63 );

	struct NewPixieAccumulatePushConstants
	{
		uint32_t sourceTextureIndex = 0;
		uint32_t historyTextureIndex = 0;
		float persistence = 0.0f;
		float padding = 0.0f;
	};

	struct NewPixieBlurPushConstants
	{
		uint32_t sourceTextureIndex = 0;
		uint32_t unusedTextureIndex = 0;
		float stepX = 0.0f;
		float stepY = 0.0f;
	};

}
