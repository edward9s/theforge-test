#pragma once
#include "Math.h"
#include <array>
#include <vector>

namespace forest {
enum class Effect { Fire, Blast, Spray, Lightning, Smoke, Water, Count };
struct EffectVertex {
    float3 position;
    float2 uv;
    float4 color;
    float3 normal;
    float4 tangent;
};
using EffectMeshes = std::array<std::vector<EffectVertex>, size_t(Effect::Count)>;
// Water surface and spray share one site; the five displays are spatially separate.
float3 effectOrigin(Effect effect);
float blastAge(float seconds);
float lightningPulse(float seconds);
// Local point-light output in lumens, synchronized with the corresponding effect.
float effectLightIntensity(Effect effect, float seconds);
// Invisible explosion/strike batches are empty; active batches keep fixed topology.
EffectMeshes effectMeshes(float seconds, float3 eye);
} // namespace forest
