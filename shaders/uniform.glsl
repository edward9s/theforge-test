layout(set=0,binding=0,std140) uniform Frame{mat4 vp;mat4 inverseVp;mat4 light[3];vec4 eyeTime;vec4 screenFps;vec4 params;vec4 sh[9];vec4 points[3];vec4 colors[3];}u;
layout(set=0,binding=1)uniform texture2D albedoTex;
layout(set=0,binding=2)uniform texture2D normalTex;
layout(set=0,binding=3)uniform texture2D armTex;
layout(set=0,binding=4)uniform texture2D shadow0;
layout(set=0,binding=5)uniform texture2D shadow1;
layout(set=0,binding=6)uniform texture2D shadow2;
layout(set=0,binding=7)uniform texture2D environmentTex;
layout(set=0,binding=8)uniform sampler linearSampler;
layout(set=0,binding=9)uniform sampler clampSampler;
layout(set=0,binding=10)uniform texture2D sceneTex;
layout(set=0,binding=11)uniform texture2D depthTex;
layout(set=0,binding=12)uniform sampler shadowSampler;
const float PI=3.14159265359;
vec2 envUv(vec3 d){return vec2(atan(d.z,d.x)/(2.0*PI)+.5,acos(clamp(d.y,-1.0,1.0))/PI);}
