#version 450
#extension GL_GOOGLE_include_directive:require
#include "uniform.glsl"
layout(location=3)in vec2 texcoord;
void main(){if(int(u.params.x)==4&&texture(sampler2D(albedoTex,linearSampler),texcoord).a<.4)discard;}
