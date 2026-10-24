#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
compiler="$PWD/.deps/forge/Common_3/Graphics/ThirdParty/OpenSource/VulkanSDK/bin/Linux/glslangValidator"
chmod +x "$compiler"
mkdir -p .generated/assets/shaders
for file in scene.vert scene.frag shadow.frag effects.frag fullscreen.vert post.frag; do
  "$compiler" -V --target-env vulkan1.1 -Ishaders "shaders/$file" -o ".generated/assets/shaders/$file.spv"
done
"$compiler" -V --target-env vulkan1.1 -Ishaders -DSHADOW=1 shaders/scene.vert -o .generated/assets/shaders/shadow.vert.spv
