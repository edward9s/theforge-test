#pragma once
#include "Math.h"
#include <cstdint>
#include <vector>

namespace forest {
using namespace glm;
struct Vertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
    float4 color;
};
enum class Surface { Ground, Stone, Bark, Leaves, Canopy };
struct Mesh {
    Surface surface;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
// Merge only bit-identical vertex records; preserve every triangle and attribute seam.
void compact(Mesh& mesh);
float elevation(float x, float z);
std::vector<Mesh> generate();
}

