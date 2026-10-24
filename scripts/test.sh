#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
for test in geometry effects; do
 g++ -std=c++20 -O2 -Wall -Wextra -Werror -I.deps/glm -Isrc "tests/$test.cpp" src/Geometry.cpp src/EffectsGeometry.cpp -o "$output/$test"
 "$output/$test"
done
