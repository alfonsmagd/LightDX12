#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace desktop_retro
{
	enum class RetroPreset : uint32_t
	{
		Crt,
		Amber,
		Green,
		Ps2Clean,
		NewPixie
	};
	struct EffectSettings
	{
		RetroPreset preset = RetroPreset::Ps2Clean;
		float intensity = 1.0f;
		float curvature = 0.032f;
		float scanlineStrength = 0.48f;
		float chromaticAberrationPixels = 0.65f;
		float noiseStrength = 0.0f;
	};
	struct PresetDefinition
	{
		const wchar_t* name;
		EffectSettings settings;
	};
	inline constexpr std::array<PresetDefinition, 5> kPresets{ { { L"Simple CRTV", { RetroPreset::Crt, .94f, .015f, .32f, .20f, .025f } },
		{ L"Amber terminal", { RetroPreset::Amber, .84f, .010f, .20f, .05f, .010f } },
		{ L"Green phosphor", { RetroPreset::Green, .90f, .012f, .32f, .04f, .016f } },
		{ L"PS2 Clean (480i)", { RetroPreset::Ps2Clean, 1.f, .032f, .48f, .65f, 0.f } },
		{ L"PS2 NewPixie CRT", { RetroPreset::NewPixie, 1.f, .038f, .58f, 0.f, 0.f } } } };
	inline const PresetDefinition& GetPreset( RetroPreset preset )
	{
		return kPresets.at( static_cast<std::size_t>( preset ) );
	}
}
