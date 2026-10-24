#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .deps
fetch(){
 local name="$1" url="$2" digest="$3"
 if [[ ! -f ".deps/$name.tar.gz" ]]; then
  curl -fLsS --retry 3 --connect-timeout 20 --max-time 300 "$url" -o ".deps/$name.tar.gz.partial"
  mv ".deps/$name.tar.gz.partial" ".deps/$name.tar.gz"
 fi
 printf '%s  %s\n' "$digest" ".deps/$name.tar.gz" | sha256sum -c -
 if [[ ! -d ".deps/$name" ]]; then
  mkdir ".deps/$name"
  tar --no-same-owner -xzf ".deps/$name.tar.gz" --strip-components=1 -C ".deps/$name"
 fi
}
fetch forge https://codeload.github.com/ConfettiFX/The-Forge/tar.gz/cd5046893faba2dc7869243873bf01f02a6f0df9 21ac9381f9711de6fa8723704533068b60abc8a02600aa69b608b859045b90ba
fetch glm https://codeload.github.com/g-truc/glm/tar.gz/refs/tags/1.0.1 9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c
python3 scripts/configure-forge.py
