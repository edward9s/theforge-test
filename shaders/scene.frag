#version 450
#extension GL_GOOGLE_include_directive:require
#include "uniform.glsl"
layout(location=0)in vec3 world;layout(location=1)in vec3 n;layout(location=2)in vec4 t;layout(location=3)in vec2 texcoord;layout(location=4)in vec4 tint;
layout(location=0)out vec4 outputColor;
float shadowSample(int c,vec2 uv){if(c==0)return texture(sampler2D(shadow0,shadowSampler),uv).r;if(c==1)return texture(sampler2D(shadow1,shadowSampler),uv).r;return texture(sampler2D(shadow2,shadowSampler),uv).r;}
float visibility(vec3 N,vec3 L){float depth=(u.vp*vec4(world,1)).w;int first=depth<7.9?0:depth<24.76?1:2;if(depth>65.0)return 1.0;
for(int c=first;c<3;c++){vec4 p=u.light[c]*vec4(world,1);vec3 q=p.xyz/p.w;vec2 uv=q.xy*vec2(.5,-.5)+.5;if(any(lessThan(uv,vec2(.01)))||any(greaterThan(uv,vec2(.99)))||q.z<0.0||q.z>1.0)continue;float bias=.00015+.00045*(1.0-max(dot(N,L),0.0));float lit=0;for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)lit+=q.z-bias<=shadowSample(c,uv+vec2(x,y)/2048.0)?1.0:0.0;return lit/9.0;}return 1.0;}
vec3 irradiance(vec3 N){return max(vec3(0),u.sh[0].rgb*.282095+.488603*(u.sh[1].rgb*N.y+u.sh[2].rgb*N.z+u.sh[3].rgb*N.x)+1.092548*(u.sh[4].rgb*N.x*N.y+u.sh[5].rgb*N.y*N.z+u.sh[7].rgb*N.x*N.z)+.315392*u.sh[6].rgb*(3.0*N.z*N.z-1.0)+.546274*u.sh[8].rgb*(N.x*N.x-N.y*N.y));}
vec3 brdf(vec3 base,float rough,float metal,vec3 N,vec3 V,vec3 L){vec3 H=normalize(V+L);float nl=max(dot(N,L),0.0),nv=max(dot(N,V),.001),nh=max(dot(N,H),0.0);float a=rough*rough,a2=a*a,d=nh*nh*(a2-1.0)+1.0;float D=a2/max(PI*d*d,.00001),k=(rough+1.0)*(rough+1.0)/8.0;float G=nv/(nv*(1.0-k)+k)*nl/(nl*(1.0-k)+k);vec3 F=mix(vec3(.04),base,metal)+(1.0-mix(vec3(.04),base,metal))*pow(1.0-max(dot(V,H),0.0),5.0);return((1.0-F)*(1.0-metal)*base/PI+D*G*F/max(4.0*nv*nl,.0001))*nl;}
void main(){int kind=int(u.params.x);vec3 N=normalize(n);if(!gl_FrontFacing)N=-N;vec3 T=normalize(t.xyz-N*dot(N,t.xyz)),B=normalize(cross(N,T))*t.w;vec3 base=tint.rgb;float rough=.68,metal=0,ao=1,alpha=1;
if(kind<3){base*=texture(sampler2D(albedoTex,linearSampler),texcoord).rgb;vec3 arm=texture(sampler2D(armTex,linearSampler),texcoord).rgb;ao=arm.r;rough=clamp(arm.g,.06,1.0);metal=arm.b;N=normalize(mat3(T,B,N)*(texture(sampler2D(normalTex,linearSampler),texcoord).xyz*2.0-1.0));}
else if(kind==4){vec4 pine=texture(sampler2D(albedoTex,linearSampler),texcoord);if(pine.a<.4)discard;base=pine.rgb;rough=.8;}
else if(kind==3){float vein=smoothstep(.015,.045,abs(texcoord.x-.5));float ribs=.94+.06*sin((texcoord.y+abs(texcoord.x-.5)*.6)*100.0);base*=mix(1.25,ribs,vein);}
else if(kind==10){float a=texcoord.y*30.0-u.eyeTime.w*4.2+sin(texcoord.x*15.0+u.eyeTime.w)*.4,b=texcoord.x*25.0+texcoord.y*19.0-u.eyeTime.w*3.1;N=normalize(mat3(T,B,N)*normalize(vec3(cos(b)*.12,cos(a)*.18,1)));float foam=pow(max(0.0,sin(a)*sin(b)),12.0)*.18;base=mix(base,vec3(.72,.85,.82),foam);rough=.24;alpha=tint.a;if(alpha>.6)alpha*=smoothstep(0.0,.12,texcoord.x)*(1.0-smoothstep(.88,1.0,texcoord.x));}
vec3 V=normalize(u.eyeTime.xyz-world),L=normalize(vec3(.32,.82,.47));float shadow=visibility(N,L);vec3 light=brdf(base,rough,metal,N,V,L)*vec3(1,.88,.70)*110000.0*shadow;light+=base*(1.0-metal)*irradiance(N)*9000.0/PI*ao;
vec3 F0=mix(vec3(.04),base,metal);vec3 env=textureLod(sampler2D(environmentTex,linearSampler),envUv(reflect(-V,N)),rough*8.0).rgb;light+=env*9000.0*(F0+(1.0-F0)*pow(1.0-max(dot(N,V),0.0),5.0))*ao;
if(kind==3||kind==4)light+=base*vec3(1,.88,.7)*110000.0*.12*pow(max(dot(-N,L),0.0),2.0)*shadow;
for(int i=0;i<3;i++){vec3 d=u.points[i].xyz-world;float r2=max(dot(d,d),.04);float fall=pow(clamp(1.0-r2/36.0,0.0,1.0),2.0);light+=brdf(base,rough,metal,N,V,normalize(d))*u.colors[i].rgb*u.points[i].w/(4.0*PI*r2)*fall;}
float fog=1.0-exp(-max(length(world-u.eyeTime.xyz)-18.0,0.0)*.004*exp(-max(world.y-1.0,0.0)*.2));outputColor=vec4(mix(light/38400.0,vec3(.36,.43,.44)*.3,fog),alpha);}
