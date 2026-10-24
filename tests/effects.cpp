#include "EffectsGeometry.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main() {
    const forest::float3 eye{3.07f, 3.16f, 9.54f};
    const size_t counts[] = {312, 432, 180, 144, 208, 1056};
    for (float t : {0.f, .12f, .3f, 1.f, 2.6f, 4.81f, 5.5f, 100.f}) {
        auto meshes = forest::effectMeshes(t, eye);
        for (size_t i = 0; i < meshes.size(); i++) {
            if (meshes[i].empty()) {
                if (i != 1 && i != 3)
                    throw std::runtime_error("Missing persistent effect");
                continue;
            }
            if (meshes[i].size() != counts[i])
                throw std::runtime_error("Effect topology changed");
            for (auto v : meshes[i])
                for (float x : {v.position.x, v.position.y, v.position.z, v.uv.x, v.uv.y, v.color.a})
                    if (!std::isfinite(x))
                        throw std::runtime_error("Nonfinite effect vertex");
        }
    }
    bool rejected = false;
    try {
        forest::effectMeshes(-1, eye);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    if (!rejected)
        throw std::runtime_error("Negative time accepted");
    std::cout << "Five effect sites and timed visibility validated\n";
}
