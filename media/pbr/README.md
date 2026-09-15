# PBR environment

These three unmodified files come from the user's local copy of [3D Graphics Rendering Cookbook, Second Edition](https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition), under `data/`:

- `piazza_bologni_1k_prefilter.ktx`: RGBA32F specular environment, nine roughness levels, six cube faces per level.
- `piazza_bologni_1k_irradiance.ktx`: RGBA32F diffuse irradiance cube.
- `brdfLUT.ktx`: RGBA16F split-sum BRDF lookup table.

The source HDRI is [Piazza Bologni by Andreas Mischok / Poly Haven](https://polyhaven.com/a/piazza_bologni), published under CC0. The reference chapter is `Chapter06/04_MetallicRoughness`; its MIT notice is in `THIRD_PARTY_NOTICES.md`.

The Ldx12 shader adapts the chapter's environment-lighting approach to HLSL and Ldx12's bindless API. The environment files are sample assets, not NuGet package contents.
