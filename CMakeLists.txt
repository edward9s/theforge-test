cmake_minimum_required(VERSION 3.22)
project(ForestForge LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
  message(FATAL_ERROR "Requires Android arm64-v8a")
endif()
set(FORGE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.deps/forge)
set(F ${FORGE_ROOT}/Common_3)
add_library(main SHARED src/Main.cpp src/Renderer.cpp src/Geometry.cpp src/EffectsGeometry.cpp
  ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
  ${F}/Graphics/GraphicsConfig.cpp ${F}/Graphics/Vulkan/Vulkan.c ${F}/Graphics/Vulkan/Vulkan_cxx.cpp ${F}/Graphics/Vulkan/VulkanRaytracing.c
  ${F}/Resources/ResourceLoader/ResourceLoader.cpp
  ${F}/OS/Android/AndroidFileSystem.cpp ${F}/OS/Android/AndroidLog.c ${F}/OS/Android/AndroidThread.c ${F}/OS/Android/AndroidTime.c
  ${F}/Utilities/FileSystem/FileSystem.c ${F}/Utilities/FileSystem/UnixFileSystem.c
  ${F}/Utilities/Log/Log.c ${F}/Utilities/MemoryTracking/MemoryTracking.c
  ${F}/Utilities/Math/Algorithms.c ${F}/Utilities/Math/StbDs.c
  ${F}/Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.c ${F}/Utilities/Threading/ThreadSystem.c ${F}/Utilities/Timer.c)
target_include_directories(main PRIVATE ${FORGE_ROOT} ${CMAKE_CURRENT_SOURCE_DIR}/.deps/glm src ${ANDROID_NDK}/sources/android/native_app_glue)
target_compile_definitions(main PRIVATE _GNU_SOURCE)
target_compile_options(main PRIVATE -ffunction-sections -fdata-sections -fvisibility=hidden)
target_link_options(main PRIVATE -Wl,--gc-sections -Wl,-z,max-page-size=16384 -Wl,--no-undefined -u ANativeActivity_onCreate)
target_link_libraries(main PRIVATE android log dl)
# Android Clang/LLD support ThinLTO directly; avoid CMake 3.22's host-Gold probe.
target_compile_options(main PRIVATE "$<$<CONFIG:Release>:-flto=thin>")
target_link_options(main PRIVATE "$<$<CONFIG:Release>:-flto=thin>")
