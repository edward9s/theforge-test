from pathlib import Path
root=Path(__file__).resolve().parents[1]/'.deps/forge/Common_3'
def disable(path,names):
    text=path.read_text()
    for name in names:
        old=f'#define {name}\n';new=f'// ForestForge disables {name}\n'
        if new in text:continue
        if text.count(old)!=1:raise RuntimeError(f'Pinned configuration changed: {name}')
        text=text.replace(old,new)
    path.write_text(text)
disable(root/'Application/Config.h',['ENABLE_FORGE_SCRIPTING','ENABLE_FORGE_RELOAD_SHADER','ENABLE_FORGE_UI','ENABLE_FORGE_FONTS','ENABLE_FORGE_INPUT','ENABLE_FORGE_TOUCH_INPUT','ENABLE_FORGE_ANIMATION_DEBUG','ENABLE_FORGE_MATERIALS','ENABLE_ZIP_FILESYSTEM','ENABLE_SCREENSHOT','ENABLE_PROFILER','ENABLE_MESHOPTIMIZER','ENABLE_THREAD_PERFORMANCE_STATS','ENABLE_FORGE_ANDROID_SHADERC'])
disable(root/'Graphics/Vulkan/VulkanConfig.h',['GFX_ENABLE_SWAPPY','VK_RAYTRACING_AVAILABLE'])
