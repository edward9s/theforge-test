#version 450
#extension GL_GOOGLE_include_directive:require
#include "uniform.glsl"
layout(location=0)in vec2 uv;layout(location=0)out vec4 outputColor;
vec3 worldAt(vec2 p,float depth){vec4 q=u.inverseVp*vec4(p*vec2(2,-2)+vec2(-1,1),depth,1);return q.xyz/q.w;}
vec3 tonemap(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.0,1.0);}
vec3 colorAt(vec2 p){float d=texture(sampler2D(depthTex,clampSampler),p).r;if(d>=.999999){vec3 dir=normalize(worldAt(p,1)-u.eyeTime.xyz);return textureLod(sampler2D(environmentTex,linearSampler),envUv(dir),0).rgb*(9000.0/38400.0);}return texture(sampler2D(sceneTex,clampSampler),p).rgb;}
uint glyph(int code){const uint digits[10]=uint[10](31599u,9362u,29671u,29391u,23497u,31183u,31215u,29257u,31727u,31695u);if(code<10)return digits[code];if(code==10)return 31204u;if(code==11)return 31716u;if(code==12)return 31183u;return 0u;}
void main(){vec2 pixel=1.0/u.screenFps.xy;vec3 c=colorAt(uv);float depth=texture(sampler2D(depthTex,clampSampler),uv).r;
if(depth<.999999){vec3 p=worldAt(uv,depth),N=normalize(cross(dFdx(p),dFdy(p)));if(dot(N,u.eyeTime.xyz-p)<0)N=-N;float occ=0;for(int i=0;i<8;i++){float a=float(i)*2.39996;vec2 offset=vec2(cos(a),sin(a))*pixel*(4.0+float(i)*2.0);float qd=texture(sampler2D(depthTex,clampSampler),uv+offset).r;vec3 delta=worldAt(uv+offset,qd)-p;float len=length(delta);if(qd<.999999&&len>.01&&len<.75)occ+=max(dot(N,delta/len)-.12,0.0)*(1.0-len/.75);}c*=1.0-clamp(occ/8.0*1.2,0.0,.55);}
vec3 glow=vec3(0);for(int i=0;i<8;i++){float a=float(i)*PI*.25;glow+=max(colorAt(uv+vec2(cos(a),sin(a))*pixel*6.0)-1.0,0.0);}c=tonemap(c+glow*(.065/8.0));
vec3 nw=tonemap(colorAt(uv+pixel*vec2(-1,-1))),ne=tonemap(colorAt(uv+pixel*vec2(1,-1))),sw=tonemap(colorAt(uv+pixel*vec2(-1,1))),se=tonemap(colorAt(uv+pixel*vec2(1,1)));vec3 luma=vec3(.299,.587,.114);float lnw=dot(nw,luma),lne=dot(ne,luma),lsw=dot(sw,luma),lse=dot(se,luma),lc=dot(c,luma);float low=min(lc,min(min(lnw,lne),min(lsw,lse))),high=max(lc,max(max(lnw,lne),max(lsw,lse)));
if(high-low>max(.04,high*.125)){vec2 dir=vec2(-(lnw+lne-lsw-lse),lnw+lsw-lne-lse);dir=clamp(dir/(min(abs(dir.x),abs(dir.y))+max((lnw+lne+lsw+lse)*.03125,.0078125)),vec2(-8),vec2(8))*pixel;c=mix(c,(tonemap(colorAt(uv-dir/6.0))+tonemap(colorAt(uv+dir/6.0)))*.5,.6);}c=pow(c,vec3(1.0/2.2));
float scale=max(2.0,floor(u.screenFps.y/240.0));vec2 h=(uv*u.screenFps.xy-vec2(16))/scale;
if(h.x>=-2&&h.x<31&&h.y>=-2&&h.y<7){c*=.2;int column=int(floor(h.x/4.0)),x=int(floor(h.x))-column*4,y=int(floor(h.y)),fps=int(clamp(round(u.screenFps.z),0,999)),code=13;if(column==0)code=10;if(column==1)code=11;if(column==2)code=12;if(column==4)code=fps/100;if(column==5)code=(fps/10)%10;if(column==6)code=fps%10;if(h.x>=0&&x<3&&y>=0&&y<5&&((glyph(code)>>uint(14-(y*3+x)))&1u)!=0u)c=vec3(1);}outputColor=vec4(c,1);}
