# Forest Ruins · The Forge

Android C++ forest/ruins comparison using The Forge's Vulkan renderer.
Reference: [edward9s/filament-test](https://github.com/edward9s/filament-test),
latest development branch `fix/effect-lights-illuminate-ruins`,
commit `4d8be26679331bef10f942e08a75d9cafdc03a5d` (recorded in `assets/reference.json`).

## Install

Run **Actions → Android test build → Run workflow**. After success, download
`TheForge-test-arm64.apk` from the rolling **test-build** prerelease.
Successful builds replace the same APK. Commits do not trigger builds; no Actions
artifacts or dependency caches are uploaded.

Requires Android 9+, arm64 and Vulkan 1.1. The application ID
`com.edward9s.theforgetest` allows installation alongside the Filament app.
Configure repository Secrets: `KEYSTORE_BASE64`, `KEY_ALIAS`, `KEY_PASSWORD`,
`STORE_PASSWORD`. Missing signing inputs fail explicitly.

One finger orbits; two fingers zoom. The initial camera, 52° vertical FOV,
view target and orbit/zoom limits match the reference. The top-left FPS display
uses half-second measurement intervals. Background time is excluded.

## Scene

The deterministic source geometry and particle trajectories are preserved:
80 × 80 m terrain, two arches, walls, paving, rubble, trees, ferns, grass, ivy,
fallen wood and spring; **232 batches, 385,622 triangles**.
Separate displays contain fire, periodic explosions, water/spray, lightning and smoke.
Fire, explosions and lightning use the latest reference’s daylight-strength point lights,
matching colors, heights, radii and intensity timing. Point-light shadow maps remain disabled.

Textures, canopy crop and HDR environment use the reference's hash-pinned CC0
sources. Tree and ruin geometry is generated in `src/Geometry.cpp`; particle and
water geometry is in `src/EffectsGeometry.cpp`. Wind runs in both color and
shadow vertex shaders. No Filament rendering code or runtime is linked.

## Rendering and comparison

The Forge manages devices, queues, uploads, swapchains, descriptor sets, pipelines,
commands and synchronization. Project shaders implement a forward GGX PBR pipeline,
normal/ARM maps, irradiance SH, GGX-prefiltered HDR reflections, alpha-cutout foliage
shadows, height fog, point lights, SSAO, bloom, filmic tone mapping and FXAA.
This implementation does not use The Forge's separate Visibility Buffer sample.

| Setting | Filament reference | This build |
|---|---|---|
| Backend | OpenGL ES | Vulkan 1.1 |
| Render scale | Dynamic 65–100% | Fixed 100% window pixels |
| Antialiasing | TAA + FXAA | FXAA |
| Sun shadows | Three 2048² cascades, 65 m | Three 2048² cascades, 65 m, 3 × 3 PCF |
| Environment | Filament cmgen | Same HDR, independently computed SH/GGX mips |
| Postprocessing | Filament | Project shaders |
| Geometry, source textures, camera | Reference | Preserved |

Images are not pixel-identical. Record camera, resolution and sustained device
temperature when comparing FPS. Actual visual quality and sustained performance
require testing on the tablet.

## Build

Pinned dependencies: The Forge `cd5046893faba2dc7869243873bf01f02a6f0df9`,
GLM 1.0.1, NDK r27d (`27.3.13750724`), CMake 3.22.1, AGP 8.9.1,
Gradle 8.12.1, Java 17.

```sh
bash scripts/deps.sh
bash scripts/test.sh
bash scripts/shaders.sh
python3 -m pip install Pillow==11.3.0 numpy==2.2.6
python3 scripts/assets.py
python3 tests/assets.py
cd android
gradle :app:assembleDebug
```

For release, set `KEYSTORE_PATH`, `STORE_PASSWORD`, `KEY_ALIAS`, `KEY_PASSWORD`
and use `:app:assembleRelease`. Release enables optimization and ThinLTO,
links libc++ statically, and uses 16 KiB native alignment.

`configure-forge.py` disables unused upstream optional modules through pinned
configuration definitions: scripting, editor UI, runtime shader compilation,
Swappy, profiling and ray-tracing feature selection. NativeActivity supplies
lifecycle/input. The ray-tracing translation unit is compiled for its descriptor
helper; ray tracing is disabled. Configuration edits fail if expected definitions change.

Validation covers deterministic geometry, vertex seams, index ranges, tangent
frames, triangle validity, particle topology/timing, complete mip chains, finite
HDR/SH values, SPIR-V compilation, Android linking, APK contents, native ABI,
entry-point export, ELF/ZIP alignment and signature. There is no automated
Android GPU render test.

See [asset credits](assets/CREDITS.md) and [third-party notices](NOTICE.md).

Android startup validates both GPU configuration mounts before initializing Vulkan.
`gpu.data` is mounted through `RD_OTHER_FILES`, and `gpu.cfg` through `RD_GPU_CONFIG`.
All PathStatement directories end with `/`, as the Android filesystem concatenates
paths literally. Asset and APK checks validate these runtime paths. On failure,
the native loop drains lifecycle events until Android requests destruction.

Presentation synchronization is allocated from the actual swapchain image count
returned by the driver, including Android surfaces requiring four or more images.
These semaphores are recreated with the swapchain; CPU frame slots remain independent.
