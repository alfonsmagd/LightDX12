cbuffer Constants : register(b0)
{
    float4x4 viewProjection;
    float4 baseColor;

    float3 emissive;
    float metallic;

    float3 cameraPosition;
    float roughness;

    float normalScale;
    float occlusionStrength;
    uint baseColorTexture;
    uint metallicRoughnessTexture;

    uint normalTexture;
    uint occlusionTexture;
    uint emissiveTexture;
    uint baseColorSampler;

    uint metallicRoughnessSampler;
    uint normalSampler;
    uint occlusionSampler;
    uint emissiveSampler;

    uint4 prefilteredTextures[3];
    uint irradianceTexture;
    uint brdfTexture;
    uint environmentSampler;
    uint environmentLevels;

    float exposure;
    uint debugView;
    float textureLodBias;
    float environmentIntensity;

    float alphaCutoff;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float3 worldPosition : POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(float3 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD0)
{
    VertexOutput output;
    output.position = mul(viewProjection, float4(position, 1.0));
    output.normal = normal;
    output.worldPosition = position;
    output.uv = uv;
    return output;
}

float4 ReadTexture(uint index, uint samplerIndex, float2 uv, float4 fallback)
{
    if (index == 0)
        return fallback;

    Texture2D<float4> texture = ResourceDescriptorHeap[index];
    SamplerState textureSampler = SamplerDescriptorHeap[samplerIndex];
    return texture.SampleBias(textureSampler, uv, textureLodBias);
}


float3 LinearToSrgb(float3 color)
{
    return select(color <= 0.0031308, 12.92 * color, 1.055 * pow(color, 1.0 / 2.4) - 0.055);
}


float3 ReadNormal(VertexOutput input)
{
    float3 normal = normalize(input.normal);

    if (normalTexture == 0)
        return normal;

    // Build the tangent frame from screen derivatives; the helmet has no TANGENT attribute.
    float3 positionDx = ddx(input.worldPosition);
    float3 positionDy = ddy(input.worldPosition);
    float2 uvDx = ddx(input.uv);
    float2 uvDy = ddy(input.uv);

    float3 perpendicularX = cross(normal, positionDx);
    float3 perpendicularY = cross(positionDy, normal);
    float3 tangent = perpendicularY * uvDx.x + perpendicularX * uvDy.x;
    float3 bitangent = perpendicularY * uvDx.y + perpendicularX * uvDy.y;
    float frameScale = rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-12));

    float3 mapped = ReadTexture(normalTexture, normalSampler, input.uv, float4(0.5, 0.5, 1, 1)).xyz * 2 - 1;
    mapped.xy *= normalScale;

    return normalize(tangent * frameScale * mapped.x + bitangent * frameScale * mapped.y + normal * mapped.z);
}


float3 ReadPrefilteredEnvironment(float3 direction, float surfaceRoughness)
{
    float lod = surfaceRoughness * (environmentLevels - 1);
    uint lowerLevel = (uint)floor(lod);
    uint upperLevel = min(lowerLevel + 1, environmentLevels - 1);

    uint lowerIndex = prefilteredTextures[lowerLevel / 4][lowerLevel % 4];
    uint upperIndex = prefilteredTextures[upperLevel / 4][upperLevel % 4];

    TextureCube<float4> lower = ResourceDescriptorHeap[NonUniformResourceIndex(lowerIndex)];
    TextureCube<float4> upper = ResourceDescriptorHeap[NonUniformResourceIndex(upperIndex)];
    SamplerState environmentFilter = SamplerDescriptorHeap[environmentSampler];

    float3 a = lower.SampleLevel(environmentFilter, direction, 0).rgb;
    float3 b = upper.SampleLevel(environmentFilter, direction, 0).rgb;

    return lerp(a, b, frac(lod));
}


// This is a direct HLSL port of Chapter06/04_MetallicRoughness/src/PBR.sp.
// Only texture access is different: ResourceDescriptorHeap/SamplerDescriptorHeap
// replace the Vulkan bindless helpers.
struct PBRInfo
{
    float NdotL;
    float NdotV;
    float NdotH;
    float LdotH;
    float VdotH;

    float3 n;
    float3 v;

    float perceptualRoughness;
    float3 reflectance0;
    float3 reflectance90;
    float alphaRoughness;
    float3 diffuseColor;
    float3 specularColor;
};


PBRInfo CalculatePBRInputsMetallicRoughness(float3 albedo, float3 normal, float3 cameraPosition,
                                             float3 worldPosition, float4 metallicRoughness)
{
    PBRInfo pbr;

    float metalness = saturate(metallic * metallicRoughness.b);
    float perceptualRoughness = clamp(roughness * metallicRoughness.g, 0.04, 1.0);
    float3 f0 = 0.04.xxx;
    float3 specularColor = lerp(f0, albedo, metalness);
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    pbr.NdotV = clamp(abs(dot(normalize(normal), normalize(cameraPosition - worldPosition))), 0.001, 1.0);
    pbr.n = normalize(normal);
    pbr.v = normalize(cameraPosition - worldPosition);
    pbr.perceptualRoughness = perceptualRoughness;
    pbr.reflectance0 = specularColor;
    pbr.reflectance90 = saturate(reflectance * 25.0).xxx;
    pbr.alphaRoughness = perceptualRoughness * perceptualRoughness;
    pbr.diffuseColor = lerp(albedo, 0.xxx, metalness);
    pbr.specularColor = specularColor;

    return pbr;
}


float3 SpecularReflection(PBRInfo pbr)
{
    return pbr.reflectance0 + (pbr.reflectance90 - pbr.reflectance0) * pow(saturate(1 - pbr.VdotH), 5);
}


float GeometricOcclusion(PBRInfo pbr)
{
    float roughnessSquared = pbr.alphaRoughness * pbr.alphaRoughness;
    float attenuationL = 2 * pbr.NdotL / (pbr.NdotL + sqrt(roughnessSquared + (1 - roughnessSquared) * pbr.NdotL * pbr.NdotL));
    float attenuationV = 2 * pbr.NdotV / (pbr.NdotV + sqrt(roughnessSquared + (1 - roughnessSquared) * pbr.NdotV * pbr.NdotV));
    return attenuationL * attenuationV;
}


float MicrofacetDistribution(PBRInfo pbr)
{
    const float pi = 3.14159265;
    float roughnessSquared = pbr.alphaRoughness * pbr.alphaRoughness;
    float denominator = (pbr.NdotH * roughnessSquared - pbr.NdotH) * pbr.NdotH + 1;
    return roughnessSquared / (pi * denominator * denominator);
}


float3 DiffuseBurley(PBRInfo pbr)
{
    const float pi = 3.14159265;
    float f90 = 2 * pbr.LdotH * pbr.LdotH * pbr.alphaRoughness - 0.5;
    return (pbr.diffuseColor / pi) * (1 + f90 * pow(1 - pbr.NdotL, 5)) * (1 + f90 * pow(1 - pbr.NdotV, 5));
}


float3 CalculatePBRLightContribution(PBRInfo pbr, float3 lightDirection, float3 lightColor)
{
    float3 light = normalize(lightDirection);
    float3 halfway = normalize(light + pbr.v);

    pbr.NdotL = clamp(dot(pbr.n, light), 0.001, 1.0);
    pbr.NdotH = saturate(dot(pbr.n, halfway));
    pbr.LdotH = saturate(dot(light, halfway));
    pbr.VdotH = saturate(dot(pbr.v, halfway));

    float3 fresnel = SpecularReflection(pbr);
    float geometry = GeometricOcclusion(pbr);
    float distribution = MicrofacetDistribution(pbr);
    float3 diffuse = (1 - fresnel) * DiffuseBurley(pbr);
    float3 specular = fresnel * geometry * distribution / (4 * pbr.NdotL * pbr.NdotV);

    return pbr.NdotL * lightColor * (diffuse + specular);
}


float3 GetIBLRadianceContributionGGX(PBRInfo pbr)
{
    Texture2D<float4> brdfLut = ResourceDescriptorHeap[brdfTexture];
    SamplerState environmentFilter = SamplerDescriptorHeap[environmentSampler];

    float2 brdf = brdfLut.SampleLevel(environmentFilter, float2(pbr.NdotV, pbr.perceptualRoughness), 0).rg;

    // The environment uses glTF's original right-handed coordinates.
    float3 reflection = -normalize(reflect(pbr.v, pbr.n)) * float3(1, 1, -1);
    float3 specularLight = ReadPrefilteredEnvironment(reflection, pbr.perceptualRoughness);
    float3 fresnelRoughness = max((1 - pbr.perceptualRoughness).xxx, pbr.reflectance0) - pbr.reflectance0;
    float3 kS = pbr.reflectance0 + fresnelRoughness * pow(1 - pbr.NdotV, 5);
    float3 FssEss = kS * brdf.x + brdf.y;

    return specularLight * FssEss * environmentIntensity;
}


float3 GetIBLRadianceLambertian(PBRInfo pbr)
{
    TextureCube<float4> irradianceMap = ResourceDescriptorHeap[irradianceTexture];
    Texture2D<float4> brdfLut = ResourceDescriptorHeap[brdfTexture];
    SamplerState environmentFilter = SamplerDescriptorHeap[environmentSampler];

    float2 fAb = brdfLut.SampleLevel(environmentFilter, float2(pbr.NdotV, pbr.perceptualRoughness), 0).rg;
    float3 irradiance = irradianceMap.SampleLevel(environmentFilter, pbr.n * float3(1, 1, -1), 0).rgb;
    float3 fresnelRoughness = max((1 - pbr.perceptualRoughness).xxx, pbr.reflectance0) - pbr.reflectance0;
    float3 kS = pbr.reflectance0 + fresnelRoughness * pow(1 - pbr.NdotV, 5);
    float3 FssEss = kS * fAb.x + fAb.y;
    float Ems = 1 - (fAb.x + fAb.y);
    float3 Favg = pbr.reflectance0 + (1 - pbr.reflectance0) / 21;
    float3 FmsEms = Ems * FssEss * Favg / (1 - Favg * Ems);
    float3 kD = pbr.diffuseColor * (1 - FssEss + FmsEms);

    return (FmsEms + kD) * irradiance * environmentIntensity;
}


float4 PSMain(VertexOutput input, bool frontFace : SV_IsFrontFace) : SV_Target0
{
    // Color maps are sRGB textures: hardware decodes them before filtering.
    float4 baseSample = baseColor * ReadTexture(baseColorTexture, baseColorSampler, input.uv, 1.xxxx);
    float3 albedo = baseSample.rgb;

    // glTF stores roughness in G and metallic in B, both linear.
    float4 metalRough = ReadTexture(metallicRoughnessTexture, metallicRoughnessSampler, input.uv, 1.xxxx);
    float3 normal = ReadNormal(input);
    normal = frontFace ? normal : -normal;
    clip(baseSample.a - alphaCutoff);
    PBRInfo pbr = CalculatePBRInputsMetallicRoughness(albedo, normal, cameraPosition, input.worldPosition, metalRough);

    // Kept in the same order as the reference PBR sample: AO modulates the
    // accumulated lighting and emission is added afterwards.
    float occlusion = lerp(1, ReadTexture(occlusionTexture, occlusionSampler, input.uv, 1.xxxx).r, occlusionStrength);
    float3 emission = emissive * ReadTexture(emissiveTexture, emissiveSampler, input.uv, 1.xxxx).rgb;

    float3 color = GetIBLRadianceContributionGGX(pbr) + GetIBLRadianceLambertian(pbr);
    color += CalculatePBRLightContribution(pbr, normalize(float3(0, 0, -5) - input.worldPosition), 1.xxx);
    color *= occlusion < 0.01 ? 1 : occlusion;
    color += emission;

    if (debugView == 1) return float4(LinearToSrgb(albedo), 1);
    if (debugView == 2) return float4(normal * 0.5 + 0.5, 1);
    if (debugView == 3) return float4(pbr.perceptualRoughness.xxx, 1);
    if (debugView == 4) return float4(metallic * metalRough.b.xxx, 1);
    if (debugView == 5) return float4(occlusion.xxx, 1);
    if (debugView == 6) return float4(LinearToSrgb(emission), 1);

    if (debugView == 7 && baseColorTexture != 0)
    {
        Texture2D<float4> albedoMap = ResourceDescriptorHeap[baseColorTexture];
        SamplerState albedoFilter = SamplerDescriptorHeap[baseColorSampler];
        float mip = max(0, albedoMap.CalculateLevelOfDetail(albedoFilter, input.uv) + textureLodBias);
        return float4(saturate(mip / 8), 1 - saturate(mip / 8), 0, 1);
    }

    // Preserve HDR lighting until the final exposure and display conversion.
    color *= exposure;
    color = color / (1 + color);
    return float4(LinearToSrgb(color), 1);
}
