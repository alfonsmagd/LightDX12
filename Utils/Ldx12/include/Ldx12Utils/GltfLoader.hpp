#pragma once

#include "Ldx12Utils/Geometry.hpp"
#include "Ldx12Utils/TextureLoader.hpp"

#include <filesystem>
#include <vector>

namespace ldx12::utils
{
	struct GltfTexture
	{
		// Index into GltfScene::images; -1 means no texture.
		int32_t image = -1;
		SamplerDesc sampler;
	};


	struct GltfPrimitive
	{
		std::vector<GeometryVertex> vertices;
		std::vector<uint32_t> indices;

		DirectX::XMFLOAT4 baseColor = { 1, 1, 1, 1 };
		DirectX::XMFLOAT3 emissive = {};

		float metallic = 1;
		float roughness = 1;
		float normalScale = 1;
		float occlusionStrength = 1;
		float alphaCutoff = -1; // Negative means opaque; MASK uses the material's cutoff.

		GltfTexture baseColorTexture;
		GltfTexture metallicRoughnessTexture;
		GltfTexture normalTexture;
		GltfTexture occlusionTexture;
		GltfTexture emissiveTexture;
	};

	struct GltfScene
	{
		std::vector<GltfPrimitive> primitives;
		std::vector<ImageRgba8> images;
		DirectX::XMFLOAT3 boundsMin = {};
		DirectX::XMFLOAT3 boundsMax = {};
	};

	// Loads the default scene (or the first scene) without requiring a GPU.
	// Static triangle meshes with opaque metallic/roughness materials and TEXCOORD_0.
	// Each texture carries its glTF sampler. Images may be external or GLB buffer views.
	// Node transforms are baked into vertices, converted to left-handed coordinates.
	// Demo scope: no animation, skinning, compression, alpha blending or extensions.
	GltfScene LoadGltfScene( const std::filesystem::path& path );
}
