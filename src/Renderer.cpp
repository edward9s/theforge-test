#include "Renderer.h"
#include "Geometry.h"
#include "EffectsGeometry.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <android/asset_manager.h>
#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "Common_3/Utilities/Interfaces/IFileSystem.h"
#include "Common_3/Utilities/Interfaces/IMemory.h"
#include "Common_3/Utilities/Log/Log.h"
namespace {
constexpr unsigned frames=2,sets=27,uniformStride=768,shadowSize=2048;
struct Uniform{glm::mat4 vp,inverseVp,light[3];glm::vec4 eyeTime,screenFps,params,sh[9],points[3],colors[3];};
static_assert(sizeof(Uniform)<=uniformStride);
struct Batch{unsigned first,count,base,material;glm::vec3 lo,hi;};
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool visible(const Batch& b,const glm::mat4& matrix){
    glm::vec4 p[8];for(int i=0;i<8;i++)p[i]=matrix*glm::vec4(i&1?b.hi.x:b.lo.x,i&2?b.hi.y:b.lo.y,i&4?b.hi.z:b.lo.z,1);
    for(int plane=0;plane<6;plane++){bool outside=true;for(auto q:p){float d=plane==0?q.x+q.w:plane==1?q.w-q.x:plane==2?q.y+q.w:plane==3?q.w-q.y:plane==4?q.z:q.w-q.z;if(d>=0){outside=false;break;}}if(outside)return false;}return true;
}
}
struct ForestRenderer::Impl{
    ANativeActivity* activity;::Renderer* renderer{};Queue* queue{};SwapChain* swap{};
    Semaphore* acquired[frames]{},*finished[3]{};Fence* fence[frames]{};CmdPool* pool[frames]{};Cmd* cmd[frames]{};
    Buffer* uniform[frames]{},*dynamicVertices[frames]{},*dynamicIndices[frames]{},*vertices{},*indices{};
    Texture* textures[11]{};RenderTarget* shadows[3]{},*hdr{},*depth{};
    Sampler* linear{},*clamp{},*shadowSampler{};Shader* shaders[4]{};Pipeline* pipelines[6]{};
    Descriptor descriptors[13]{};DescriptorSetLayoutDesc layout{};DescriptorSet* bindings{};
    std::vector<Batch> batches;glm::vec4 sh[9]{};unsigned frameIndex{},width{},height{};bool submitted[frames]{};
    bool memoryReady{},filesReady{},logReady{},gpuReady{},loaderReady{};
    explicit Impl(ANativeActivity* a):activity(a){}
    std::vector<uint8_t> asset(const std::string& name){
        AAsset* a=AAssetManager_open(activity->assetManager,name.c_str(),AASSET_MODE_BUFFER);
        if(!a)throw std::runtime_error("Missing asset: "+name);
        std::vector<uint8_t> data(static_cast<size_t>(AAsset_getLength64(a)));int read=AAsset_read(a,data.data(),data.size());AAsset_close(a);
        if(read<0||size_t(read)!=data.size())throw std::runtime_error("Truncated asset: "+name);return data;
    }
    Buffer* buffer(uint64_t bytes,DescriptorType type,const void* data=nullptr,bool dynamic=false){
        Buffer* b{};BufferLoadDesc load{};load.ppBuffer=&b;load.pData=data;load.mDesc.mSize=bytes;load.mDesc.mDescriptors=type;
        load.mDesc.mMemoryUsage=dynamic?RESOURCE_MEMORY_USAGE_CPU_TO_GPU:RESOURCE_MEMORY_USAGE_GPU_ONLY;
        load.mDesc.mFlags=dynamic?BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT:BUFFER_CREATION_FLAG_NONE;
        load.mDesc.mStartState=type==DESCRIPTOR_TYPE_INDEX_BUFFER?RESOURCE_STATE_INDEX_BUFFER:RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        addResource(&load,nullptr);waitForAllResourceLoads();require(b,"Buffer allocation failed");return b;
    }
    void upload(Buffer* b,const void* data,size_t size){BufferUpdateDesc d{};d.pBuffer=b;d.mSize=size;beginUpdateResource(&d);memcpy(d.pMappedData,data,size);endUpdateResource(&d);}
    Texture* texture(const char* name,bool srgb=false,bool half=false){
        auto bytes=asset(std::string("forest/")+name+".tex");require(bytes.size()>=16,"Texture header missing");uint32_t header[4];memcpy(header,bytes.data(),16);
        require(header[0]==0x31544646&&header[1]&&header[2]&&header[3],"Invalid texture header");
        TextureDesc d{};d.mWidth=header[1];d.mHeight=header[2];d.mMipLevels=header[3];d.mDepth=d.mArraySize=1;d.mSampleCount=SAMPLE_COUNT_1;d.mDescriptors=DESCRIPTOR_TYPE_TEXTURE;
        d.mFormat=half?TinyImageFormat_R16G16B16A16_SFLOAT:srgb?TinyImageFormat_R8G8B8A8_SRGB:TinyImageFormat_R8G8B8A8_UNORM;d.mStartState=RESOURCE_STATE_SHADER_RESOURCE;
        Texture* tex{};TextureLoadDesc load{};load.ppTexture=&tex;load.pDesc=&d;addResource(&load,nullptr);waitForAllResourceLoads();require(tex,"Texture allocation failed");
        TextureUpdateDesc update{};update.pTexture=tex;update.mMipLevels=d.mMipLevels;update.mLayerCount=1;update.mCurrentState=RESOURCE_STATE_SHADER_RESOURCE;beginUpdateResource(&update);
        size_t offset=16;unsigned w=d.mWidth,h=d.mHeight;
        for(unsigned mip=0;mip<d.mMipLevels;mip++){auto sub=update.getSubresourceUpdateDesc(mip,0);size_t row=w*(half?8:4);require(offset+row*h<=bytes.size(),"Texture mip truncated");for(unsigned y=0;y<h;y++)memcpy(sub.pMappedData+y*sub.mDstRowStride,bytes.data()+offset+y*row,row);offset+=row*h;w=std::max(1u,w/2);h=std::max(1u,h/2);}
        require(offset==bytes.size(),"Unexpected texture payload");endUpdateResource(&update);return tex;
    }
    Shader* shader(const char* vert,const char* frag){
        auto v=asset(std::string("shaders/")+vert+".spv"),f=asset(std::string("shaders/")+frag+".spv");BinaryShaderDesc d{};d.mStages=ShaderStage(SHADER_STAGE_VERT|SHADER_STAGE_FRAG);
        d.mVert.pByteCode=v.data();d.mVert.mByteCodeSize=v.size();d.mVert.pEntryPoint="main";d.mFrag.pByteCode=f.data();d.mFrag.mByteCodeSize=f.size();d.mFrag.pEntryPoint="main";
        Shader* s{};addShaderBinary(renderer,&d,&s);require(s,"Shader creation failed");return s;
    }
    void init(){
        require(initMemAlloc("ForestForge"),"Allocator initialization failed");memoryReady=true;
        FileSystemInitDesc fs{};fs.pAppName="ForestForge";fs.pPlatformData=activity;require(initFileSystem(&fs),"Filesystem initialization failed");filesReady=true;
        initLog("ForestForge",LogLevel(eINFO|eWARNING|eERROR));logReady=true;initGPUConfiguration(nullptr);gpuReady=true;
        RendererDesc desc{};desc.mShaderTarget=SHADER_TARGET_6_0;initRenderer("ForestForge",&desc,&renderer);require(renderer,"Vulkan 1.1 renderer unavailable");
        QueueDesc q{};q.mType=QUEUE_TYPE_GRAPHICS;initQueue(renderer,&q,&queue);initResourceLoaderInterface(renderer);loaderReady=true;
        for(unsigned i=0;i<frames;i++){
            CmdPoolDesc pd{};pd.pQueue=queue;initCmdPool(renderer,&pd,&pool[i]);CmdDesc cd{};cd.pPool=pool[i];initCmd(renderer,&cd,&cmd[i]);initFence(renderer,&fence[i]);initSemaphore(renderer,&acquired[i]);
            uniform[i]=buffer(uniformStride*sets,DESCRIPTOR_TYPE_UNIFORM_BUFFER,nullptr,true);dynamicVertices[i]=buffer(4096*sizeof(forest::Vertex),DESCRIPTOR_TYPE_VERTEX_BUFFER,nullptr,true);dynamicIndices[i]=buffer(8192*sizeof(uint16_t),DESCRIPTOR_TYPE_INDEX_BUFFER,nullptr,true);
        }
        for(auto& s:finished)initSemaphore(renderer,&s);
        std::vector<forest::Vertex> vs;std::vector<uint16_t> is;auto meshes=forest::generate();
        for(auto& m:meshes){require(m.vertices.size()<=65536,"Batch exceeds uint16 indices");Batch b{unsigned(is.size()),unsigned(m.indices.size()),unsigned(vs.size()),unsigned(m.surface),glm::vec3(1e9),glm::vec3(-1e9)};for(auto v:m.vertices){b.lo=glm::min(b.lo,v.position);b.hi=glm::max(b.hi,v.position);}b.lo-=glm::vec3(.12f);b.hi+=glm::vec3(.12f);batches.push_back(b);vs.insert(vs.end(),m.vertices.begin(),m.vertices.end());for(auto index:m.indices)is.push_back(uint16_t(index));}
        vertices=buffer(vs.size()*sizeof(vs[0]),DESCRIPTOR_TYPE_VERTEX_BUFFER,vs.data());indices=buffer(is.size()*sizeof(is[0]),DESCRIPTOR_TYPE_INDEX_BUFFER,is.data());
        const char* names[]={"ground_albedo","ground_normal","ground_arm","stone_albedo","stone_normal","stone_arm","bark_albedo","bark_normal","bark_arm","canopy","environment"};
        for(unsigned i=0;i<11;i++)textures[i]=texture(names[i],i==0||i==3||i==6||i==9,i==10);
        auto shBytes=asset("forest/irradiance.bin");require(shBytes.size()==sizeof(sh),"Invalid irradiance");memcpy(sh,shBytes.data(),sizeof(sh));
        SamplerDesc sd{};sd.mMinFilter=sd.mMagFilter=FILTER_LINEAR;sd.mMipMapMode=MIPMAP_MODE_LINEAR;sd.mAddressU=sd.mAddressV=sd.mAddressW=ADDRESS_MODE_REPEAT;sd.mMaxAnisotropy=4;addSampler(renderer,&sd,&linear);
        sd.mAddressU=sd.mAddressV=sd.mAddressW=ADDRESS_MODE_CLAMP_TO_EDGE;sd.mMaxAnisotropy=1;addSampler(renderer,&sd,&clamp);sd.mMinFilter=sd.mMagFilter=FILTER_NEAREST;addSampler(renderer,&sd,&shadowSampler);
        shaders[0]=shader("scene.vert","scene.frag");shaders[1]=shader("shadow.vert","shadow.frag");shaders[2]=shader("scene.vert","effects.frag");shaders[3]=shader("fullscreen.vert","post.frag");
        for(unsigned i=0;i<13;i++){descriptors[i].mOffset=i;descriptors[i].mCount=1;descriptors[i].mType=i==0?DESCRIPTOR_TYPE_UNIFORM_BUFFER:(i==8||i==9||i==12)?DESCRIPTOR_TYPE_SAMPLER:DESCRIPTOR_TYPE_TEXTURE;}layout.pDescriptors=descriptors;layout.mDescriptorCount=13;
    }
    RenderTarget* target(unsigned w,unsigned h,TinyImageFormat format,bool isDepth){RenderTargetDesc d{};d.mWidth=w;d.mHeight=h;d.mDepth=d.mArraySize=d.mMipLevels=1;d.mFormat=format;d.mSampleCount=SAMPLE_COUNT_1;d.mDescriptors=DESCRIPTOR_TYPE_TEXTURE;d.mStartState=RESOURCE_STATE_SHADER_RESOURCE;if(isDepth)d.mClearValue.depth=1;RenderTarget* r{};addRenderTarget(renderer,&d,&r);require(r,"Render target unavailable");return r;}
    void makePipeline(unsigned index,Shader* shader,bool shadow,bool blend,bool fullscreen){
        VertexLayout vl{};vl.mBindingCount=1;vl.mBindings[0].mStride=sizeof(forest::Vertex);vl.mAttribCount=5;
        const unsigned offsets[]={offsetof(forest::Vertex,position),offsetof(forest::Vertex,normal),offsetof(forest::Vertex,tangent),offsetof(forest::Vertex,uv),offsetof(forest::Vertex,color)};
        for(unsigned i=0;i<5;i++){vl.mAttribs[i].mLocation=i;vl.mAttribs[i].mOffset=offsets[i];vl.mAttribs[i].mFormat=i==3?TinyImageFormat_R32G32_SFLOAT:(i==2||i==4)?TinyImageFormat_R32G32B32A32_SFLOAT:TinyImageFormat_R32G32B32_SFLOAT;}
        RasterizerStateDesc raster{};raster.mCullMode=index==0?CULL_MODE_BACK:CULL_MODE_NONE;DepthStateDesc ds{};ds.mDepthTest=!fullscreen;ds.mDepthWrite=!fullscreen&&!blend;ds.mDepthFunc=CMP_LEQUAL;
        BlendStateDesc bs{};bs.mRenderTargetMask=BLEND_STATE_TARGET_0;bs.mColorWriteMasks[0]=COLOR_MASK_ALL;bs.mSrcFactors[0]=BC_SRC_ALPHA;bs.mDstFactors[0]=BC_ONE_MINUS_SRC_ALPHA;bs.mBlendModes[0]=BM_ADD;bs.mSrcAlphaFactors[0]=BC_ONE;bs.mDstAlphaFactors[0]=BC_ONE_MINUS_SRC_ALPHA;bs.mBlendAlphaModes[0]=BM_ADD;
        TinyImageFormat format=fullscreen?swap->ppRenderTargets[0]->mFormat:TinyImageFormat_R16G16B16A16_SFLOAT;PipelineDesc pd{};pd.mType=PIPELINE_TYPE_GRAPHICS;const DescriptorSetLayoutDesc* layouts[]={&layout};pd.pLayouts=layouts;pd.mLayoutCount=1;
        auto& g=pd.mGraphicsDesc;g.pShaderProgram=shader;g.pVertexLayout=fullscreen?nullptr:&vl;g.pRasterizerState=&raster;g.pDepthState=&ds;g.pBlendState=blend?&bs:nullptr;g.pColorFormats=shadow?nullptr:&format;g.mRenderTargetCount=shadow?0:1;g.mDepthStencilFormat=fullscreen?TinyImageFormat_UNDEFINED:TinyImageFormat_D32_SFLOAT;g.mSampleCount=SAMPLE_COUNT_1;g.mPrimitiveTopo=PRIMITIVE_TOPO_TRI_LIST;
        addPipeline(renderer,&pd,&pipelines[index]);require(pipelines[index],"Pipeline creation failed");
    }
    void open(ANativeWindow* window){
        close();width=ANativeWindow_getWidth(window);height=ANativeWindow_getHeight(window);require(width&&height,"Invalid window dimensions");
        SwapChainDesc d{};d.mWindowHandle.window=window;d.mWindowHandle.activity=activity;d.ppPresentQueues=&queue;d.mPresentQueueCount=1;d.mImageCount=3;d.mWidth=width;d.mHeight=height;d.mColorFormat=TinyImageFormat_R8G8B8A8_UNORM;d.mColorSpace=COLOR_SPACE_SDR_SRGB;d.mEnableVsync=true;
        addSwapChain(renderer,&d,&swap);require(swap,"Swapchain creation failed");require(swap->mImageCount<=3,"Unexpected swapchain image count");
        hdr=target(width,height,TinyImageFormat_R16G16B16A16_SFLOAT,false);depth=target(width,height,TinyImageFormat_D32_SFLOAT,true);for(auto& s:shadows)s=target(shadowSize,shadowSize,TinyImageFormat_D32_SFLOAT,true);
        makePipeline(0,shaders[0],false,false,false);makePipeline(1,shaders[1],true,false,false);makePipeline(2,shaders[2],false,true,false);makePipeline(3,shaders[0],false,true,false);makePipeline(4,shaders[3],false,false,true);makePipeline(5,shaders[0],false,false,false);
        DescriptorSetDesc dd{};dd.mMaxSets=sets*frames;dd.mDescriptorCount=13;dd.pDescriptors=descriptors;addDescriptorSet(renderer,&dd,&bindings);
        for(unsigned f=0;f<frames;f++)for(unsigned s=0;s<sets;s++){
            unsigned material=s<11?s:s<26?(s-11)%5:0,base=material<3?material*3:0;
            Texture* ts[13]={nullptr,material==4?textures[9]:textures[base],textures[base+1],textures[base+2],shadows[0]->pTexture,shadows[1]->pTexture,shadows[2]->pTexture,textures[10],nullptr,nullptr,hdr->pTexture,depth->pTexture,nullptr};
            DescriptorData data[13]{};DescriptorDataRange range{s*uniformStride,sizeof(Uniform),0};
            for(unsigned i=0;i<13;i++){data[i].mIndex=i;data[i].mCount=1;if(i==0){data[i].ppBuffers=&uniform[f];data[i].pRanges=&range;}else if(i==8)data[i].ppSamplers=&linear;else if(i==9)data[i].ppSamplers=&clamp;else if(i==12)data[i].ppSamplers=&shadowSampler;else data[i].ppTextures=&ts[i];}updateDescriptorSet(renderer,f*sets+s,bindings,13,data);
        }
    }
    void close(){if(!queue)return;waitQueueIdle(queue);if(bindings){removeDescriptorSet(renderer,bindings);bindings=nullptr;}for(auto& p:pipelines)if(p){removePipeline(renderer,p);p=nullptr;}for(auto& s:shadows)if(s){removeRenderTarget(renderer,s);s=nullptr;}if(depth){removeRenderTarget(renderer,depth);depth=nullptr;}if(hdr){removeRenderTarget(renderer,hdr);hdr=nullptr;}if(swap){removeSwapChain(renderer,swap);swap=nullptr;}}
    ~Impl(){
        close();if(queue)waitQueueIdle(queue);if(loaderReady)waitForAllResourceLoads();
        if(renderer){for(auto s:shaders)if(s)removeShader(renderer,s);for(auto t:textures)if(t)removeResource(t);for(auto s:{linear,clamp,shadowSampler})if(s)removeSampler(renderer,s);if(vertices)removeResource(vertices);if(indices)removeResource(indices);
            for(unsigned i=0;i<frames;i++){for(auto b:{uniform[i],dynamicVertices[i],dynamicIndices[i]})if(b)removeResource(b);if(cmd[i])exitCmd(renderer,cmd[i]);if(pool[i])exitCmdPool(renderer,pool[i]);if(fence[i])exitFence(renderer,fence[i]);if(acquired[i])exitSemaphore(renderer,acquired[i]);}
            for(auto s:finished)if(s)exitSemaphore(renderer,s);if(loaderReady)exitResourceLoaderInterface(renderer);if(queue)exitQueue(renderer,queue);exitRenderer(renderer);
        }
        if(gpuReady)exitGPUConfiguration();if(logReady)exitLog();if(filesReady)exitFileSystem();if(memoryReady)exitMemAlloc();
    }
    void frame(float seconds,float yaw,float pitch,float distance,float fps);
};
ForestRenderer::ForestRenderer(ANativeActivity* a):impl_(std::make_unique<Impl>(a)){impl_->init();}
ForestRenderer::~ForestRenderer()=default;void ForestRenderer::open(ANativeWindow* w){impl_->open(w);}void ForestRenderer::close(){impl_->close();}bool ForestRenderer::ready()const{return impl_->swap!=nullptr;}
void ForestRenderer::frame(float t,float y,float p,float d,float fps){impl_->frame(t,y,p,d,fps);}
void ForestRenderer::Impl::frame(float seconds,float yaw,float pitch,float distance,float fps){
    if(!swap)return;unsigned f=frameIndex;if(submitted[f])waitForFences(renderer,1,&fence[f]);uint32_t image{};acquireNextImage(renderer,swap,acquired[f],nullptr,&image);require(image<swap->mImageCount,"Swapchain acquisition failed");
    Uniform u{};float horizontal=distance*std::cos(pitch);glm::vec3 eye{horizontal*std::sin(yaw),1.6f+distance*std::sin(pitch),-3+horizontal*std::cos(yaw)};
    auto view=glm::lookAtRH(eye,glm::vec3(0,1.6f,-3),glm::vec3(0,1,0));auto projection=glm::perspectiveRH_ZO(glm::radians(52.f),float(width)/height,.1f,100.f);u.vp=projection*view;u.inverseVp=glm::inverse(u.vp);u.eyeTime={eye,seconds};u.screenFps={width,height,fps,0};memcpy(u.sh,sh,sizeof(sh));
    const float splits[]={.1f,7.9f,24.76f,65.f};auto lv=glm::lookAtRH(glm::normalize(glm::vec3(.32,.82,.47))*100.f,glm::vec3(0),glm::vec3(0,1,0));
    for(unsigned c=0;c<3;c++){auto inverse=glm::inverse(glm::perspectiveRH_ZO(glm::radians(52.f),float(width)/height,splits[c],splits[c+1])*view);glm::vec3 corners[8],center(0);for(int i=0;i<8;i++){glm::vec4 p=inverse*glm::vec4(i&1?1:-1,i&2?1:-1,i&4?1:0,1);corners[i]=glm::vec3(p)/p.w;center+=corners[i]/8.f;}float radius=0;for(auto p:corners)radius=std::max(radius,glm::length(p-center));radius=std::ceil(radius*16)/16;glm::vec3 lc=glm::vec3(lv*glm::vec4(center,1));float texel=2*radius/shadowSize;lc.x=std::floor(lc.x/texel)*texel;lc.y=std::floor(lc.y/texel)*texel;u.light[c]=glm::orthoRH_ZO(lc.x-radius,lc.x+radius,lc.y-radius,lc.y+radius,-lc.z-radius-25,-lc.z+radius+25)*lv;}
    const forest::Effect lights[]={forest::Effect::Fire,forest::Effect::Blast,forest::Effect::Lightning};const float intensities[]={9500.f*(1+.13f*std::sin(seconds*13)+.09f*std::sin(seconds*23)),80000.f*std::exp(-forest::blastAge(seconds)*7),55000.f*forest::lightningPulse(seconds)};const glm::vec3 colors[]={{1,.32,.06},{1,.5,.1},{.45,.65,1}};
    for(int i=0;i<3;i++){u.points[i]=glm::vec4(forest::effectOrigin(lights[i])+glm::vec3(0,1,0),intensities[i]);u.colors[i]=glm::vec4(colors[i],1);}
    std::array<uint8_t,uniformStride*sets> ub{};for(unsigned s=0;s<sets;s++){u.params={s<11?float(s):s<26?float((s-11)%5):0.f,s>=11&&s<26?float((s-11)/5):0.f,.1f,100.f};memcpy(ub.data()+s*uniformStride,&u,sizeof(u));}upload(uniform[f],ub.data(),ub.size());
    auto effects=forest::effectMeshes(seconds,eye);std::vector<forest::Vertex> ev;std::vector<uint16_t> ei;struct EffectDraw{unsigned first,count,material;float distance;};std::vector<EffectDraw> draws;
    for(unsigned i=0;i<effects.size();i++){if(effects[i].empty())continue;unsigned base=ev.size(),first=ei.size();for(auto v:effects[i])ev.push_back({v.position,v.normal,v.tangent,v.uv,v.color});for(unsigned j=0;j<effects[i].size();j+=4)for(unsigned k:{0u,1u,2u,0u,2u,3u})ei.push_back(uint16_t(base+j+k));draws.push_back({first,unsigned(ei.size())-first,i+5,glm::length2(forest::effectOrigin(forest::Effect(i))-eye)});}
    require(ev.size()<=4096&&ei.size()<=8192,"Effect buffer capacity exceeded");upload(dynamicVertices[f],ev.data(),ev.size()*sizeof(ev[0]));upload(dynamicIndices[f],ei.data(),ei.size()*sizeof(ei[0]));std::sort(draws.begin(),draws.end(),[](auto a,auto b){return a.distance>b.distance;});
    Cmd* command=cmd[f];resetCmdPool(renderer,pool[f]);beginCmd(command);
    auto transition=[&](RenderTarget* rt,ResourceState before,ResourceState after){RenderTargetBarrier b{};b.pRenderTarget=rt;b.mCurrentState=before;b.mNewState=after;cmdResourceBarrier(command,0,nullptr,0,nullptr,1,&b);};
    const unsigned stride=sizeof(forest::Vertex);cmdBindVertexBuffer(command,1,&vertices,&stride,nullptr);cmdBindIndexBuffer(command,indices,INDEX_TYPE_UINT16,0);
    for(unsigned c=0;c<3;c++){transition(shadows[c],RESOURCE_STATE_SHADER_RESOURCE,RESOURCE_STATE_DEPTH_WRITE);BindRenderTargetsDesc target{};target.mDepthStencil.pDepthStencil=shadows[c];target.mDepthStencil.mLoadAction=LOAD_ACTION_CLEAR;target.mDepthStencil.mStoreAction=STORE_ACTION_STORE;cmdBindRenderTargets(command,&target);cmdSetViewport(command,0,0,shadowSize,shadowSize,0,1);cmdSetScissor(command,0,0,shadowSize,shadowSize);cmdBindPipeline(command,pipelines[1]);for(auto b:batches)if(b.material!=0&&visible(b,u.light[c])){cmdBindDescriptorSet(command,f*sets+11+c*5+b.material,bindings);cmdDrawIndexed(command,b.count,b.first,b.base);}cmdBindRenderTargets(command,nullptr);transition(shadows[c],RESOURCE_STATE_DEPTH_WRITE,RESOURCE_STATE_SHADER_RESOURCE);}
    transition(hdr,RESOURCE_STATE_SHADER_RESOURCE,RESOURCE_STATE_RENDER_TARGET);transition(depth,RESOURCE_STATE_SHADER_RESOURCE,RESOURCE_STATE_DEPTH_WRITE);
    BindRenderTargetsDesc target{};target.mRenderTargetCount=1;target.mRenderTargets[0].pRenderTarget=hdr;target.mRenderTargets[0].mLoadAction=LOAD_ACTION_CLEAR;target.mRenderTargets[0].mStoreAction=STORE_ACTION_STORE;target.mDepthStencil.pDepthStencil=depth;target.mDepthStencil.mLoadAction=LOAD_ACTION_CLEAR;target.mDepthStencil.mStoreAction=STORE_ACTION_STORE;cmdBindRenderTargets(command,&target);cmdSetViewport(command,0,0,width,height,0,1);cmdSetScissor(command,0,0,width,height);
    for(auto b:batches)if(visible(b,u.vp)){cmdBindPipeline(command,pipelines[b.material>=3?5:0]);cmdBindDescriptorSet(command,f*sets+b.material,bindings);cmdDrawIndexed(command,b.count,b.first,b.base);}
    cmdBindVertexBuffer(command,1,&dynamicVertices[f],&stride,nullptr);cmdBindIndexBuffer(command,dynamicIndices[f],INDEX_TYPE_UINT16,0);for(auto d:draws){cmdBindPipeline(command,pipelines[d.material==10?3:2]);cmdBindDescriptorSet(command,f*sets+d.material,bindings);cmdDrawIndexed(command,d.count,d.first,0);}
    cmdBindRenderTargets(command,nullptr);transition(hdr,RESOURCE_STATE_RENDER_TARGET,RESOURCE_STATE_SHADER_RESOURCE);transition(depth,RESOURCE_STATE_DEPTH_WRITE,RESOURCE_STATE_SHADER_RESOURCE);auto* back=swap->ppRenderTargets[image];transition(back,RESOURCE_STATE_PRESENT,RESOURCE_STATE_RENDER_TARGET);
    BindRenderTargetsDesc output{};output.mRenderTargetCount=1;output.mRenderTargets[0].pRenderTarget=back;output.mRenderTargets[0].mLoadAction=LOAD_ACTION_DONTCARE;output.mRenderTargets[0].mStoreAction=STORE_ACTION_STORE;cmdBindRenderTargets(command,&output);cmdBindPipeline(command,pipelines[4]);cmdBindDescriptorSet(command,f*sets+26,bindings);cmdDraw(command,3,0);cmdBindRenderTargets(command,nullptr);transition(back,RESOURCE_STATE_RENDER_TARGET,RESOURCE_STATE_PRESENT);endCmd(command);
    FlushResourceUpdateDesc flush{};flushResourceUpdates(&flush);Semaphore* waits[]={flush.pOutSubmittedSemaphore,acquired[f]};QueueSubmitDesc submit{};submit.ppCmds=&command;submit.mCmdCount=1;submit.pSignalFence=fence[f];submit.ppWaitSemaphores=waits;submit.mWaitSemaphoreCount=2;submit.ppSignalSemaphores=&finished[image];submit.mSignalSemaphoreCount=1;queueSubmit(queue,&submit);submitted[f]=true;
    QueuePresentDesc present{};present.pSwapChain=swap;present.mIndex=image;present.ppWaitSemaphores=&finished[image];present.mWaitSemaphoreCount=1;present.mSubmitDone=true;queuePresent(queue,&present);frameIndex=(f+1)%frames;
}
