#pragma once

#include <cstdint>
#include <type_traits>

#define LDX12_DESCRIPTOR_SLOT_INVALID 0u

#define LDX12_FREE_CBV_SLOT_FIRST 1u
#define LDX12_FREE_CBV_SLOT_COUNT 5u
#define LDX12_ENGINE_CBV_SLOT_FIRST 6u
#define LDX12_ENGINE_CBV_SLOT_COUNT 5u
#define LDX12_CBV_SLOT_COUNT 10u
#define LDX12_CBV_SLOT_LAST 10u

#define LDX12_FREE_SRV_SLOT_FIRST 11u
#define LDX12_FREE_SRV_SLOT_COUNT 5u
#define LDX12_ENGINE_SRV_SLOT_FIRST 16u
#define LDX12_ENGINE_SRV_SLOT_COUNT 5u
#define LDX12_SRV_SLOT_COUNT 10u
#define LDX12_SRV_SLOT_LAST 20u

#define LDX12_FREE_RW_SLOT_FIRST 21u
#define LDX12_FREE_RW_SLOT_COUNT 3u
#define LDX12_ENGINE_RW_SLOT_FIRST 24u
#define LDX12_ENGINE_RW_SLOT_COUNT 2u
#define LDX12_RW_SLOT_COUNT 5u
#define LDX12_RW_SLOT_LAST 25u

#define LDX12_BINDLESS_FIXED_SLOT_FIRST 1u
#define LDX12_BINDLESS_FIXED_SLOT_LAST LDX12_RW_SLOT_LAST
#define LDX12_BINDLESS_DYNAMIC_SLOT_FIRST ( LDX12_BINDLESS_FIXED_SLOT_LAST + 1u )

namespace ldx12
{
	enum class ConstantBufferSlot : uint32_t
	{
		Invalid = LDX12_DESCRIPTOR_SLOT_INVALID,

		FreeCB0 = LDX12_FREE_CBV_SLOT_FIRST,
		FreeCB1,
		FreeCB2,
		FreeCB3,
		FreeCB4,

		EngineFrame = LDX12_ENGINE_CBV_SLOT_FIRST,
		EngineCamera,
		EngineObject,
		EngineMaterial,
		EngineLighting,
	};

	enum class ShaderResourceSlot : uint32_t
	{
		Invalid = LDX12_DESCRIPTOR_SLOT_INVALID,

		FreeSRV0 = LDX12_FREE_SRV_SLOT_FIRST,
		FreeSRV1,
		FreeSRV2,
		FreeSRV3,
		FreeSRV4,

		EngineScene = LDX12_ENGINE_SRV_SLOT_FIRST,
		EngineInstances,
		EngineMaterials,
		EngineMeshes,
		EngineTextures,
	};

	enum class ReadWriteResourceSlot : uint32_t
	{
		Invalid = LDX12_DESCRIPTOR_SLOT_INVALID,

		FreeRW0 = LDX12_FREE_RW_SLOT_FIRST,
		FreeRW1,
		FreeRW2,

		EngineScratch0 = LDX12_ENGINE_RW_SLOT_FIRST,
		EngineScratch1,
	};

	template<typename SlotType>
	concept BindingSlot =
		std::is_same_v<SlotType, ConstantBufferSlot> ||
		std::is_same_v<SlotType, ShaderResourceSlot> ||
		std::is_same_v<SlotType, ReadWriteResourceSlot>;

	template<BindingSlot SlotType>
	[[nodiscard]] constexpr uint32_t ToSlotIndex( SlotType slot ) noexcept
	{
		return static_cast<uint32_t>( slot );
	}

	[[nodiscard]] constexpr bool IsFreeConstantBufferSlot( ConstantBufferSlot slot ) noexcept
	{
		const uint32_t index = ToSlotIndex( slot );
		return index >= LDX12_FREE_CBV_SLOT_FIRST &&
			index < LDX12_FREE_CBV_SLOT_FIRST + LDX12_FREE_CBV_SLOT_COUNT;
	}

	[[nodiscard]] constexpr bool IsEngineConstantBufferSlot( ConstantBufferSlot slot ) noexcept
	{
		const uint32_t index = ToSlotIndex( slot );
		return index >= LDX12_ENGINE_CBV_SLOT_FIRST &&
			index < LDX12_ENGINE_CBV_SLOT_FIRST + LDX12_ENGINE_CBV_SLOT_COUNT;
	}

	[[nodiscard]] constexpr bool IsValidConstantBufferSlot( ConstantBufferSlot slot ) noexcept
	{
		return IsFreeConstantBufferSlot( slot ) || IsEngineConstantBufferSlot( slot );
	}

	[[nodiscard]] constexpr bool IsFreeShaderResourceSlot( ShaderResourceSlot slot ) noexcept
	{
		const uint32_t index = ToSlotIndex( slot );
		return index >= LDX12_FREE_SRV_SLOT_FIRST &&
			index < LDX12_FREE_SRV_SLOT_FIRST + LDX12_FREE_SRV_SLOT_COUNT;
	}

	[[nodiscard]] constexpr bool IsEngineShaderResourceSlot( ShaderResourceSlot slot ) noexcept
	{
		const uint32_t index = ToSlotIndex( slot );
		return index >= LDX12_ENGINE_SRV_SLOT_FIRST &&
			index < LDX12_ENGINE_SRV_SLOT_FIRST + LDX12_ENGINE_SRV_SLOT_COUNT;
	}

	[[nodiscard]] constexpr bool IsValidShaderResourceSlot( ShaderResourceSlot slot ) noexcept
	{
		return IsFreeShaderResourceSlot( slot ) || IsEngineShaderResourceSlot( slot );
	}

	[[nodiscard]] constexpr bool IsFreeReadWriteResourceSlot( ReadWriteResourceSlot slot ) noexcept
	{
		const uint32_t index = ToSlotIndex( slot );
		return index >= LDX12_FREE_RW_SLOT_FIRST &&
			index < LDX12_FREE_RW_SLOT_FIRST + LDX12_FREE_RW_SLOT_COUNT;
	}

	[[nodiscard]] constexpr bool IsEngineReadWriteResourceSlot( ReadWriteResourceSlot slot ) noexcept
	{
		const uint32_t index = ToSlotIndex( slot );
		return index >= LDX12_ENGINE_RW_SLOT_FIRST &&
			index < LDX12_ENGINE_RW_SLOT_FIRST + LDX12_ENGINE_RW_SLOT_COUNT;
	}

	[[nodiscard]] constexpr bool IsValidReadWriteResourceSlot( ReadWriteResourceSlot slot ) noexcept
	{
		return IsFreeReadWriteResourceSlot( slot ) || IsEngineReadWriteResourceSlot( slot );
	}
}
