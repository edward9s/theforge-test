#version 450
#extension GL_GOOGLE_include_directive:require
#include "uniform.glsl"
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;layout(location=2)in vec4 tangent;layout(location=3)in vec2 uv;layout(location=4)in vec4 color;
layout(location=0)out vec3 world;layout(location=1)out vec3 n;layout(location=2)out vec4 t;layout(location=3)out vec2 texcoord;layout(location=4)out vec4 tint;
void main(){world=position;n=normal;t=tangent;texcoord=uv;tint=color;int kind=int(u.params.x);
if(kind==3||kind==4){float phase=position.x*.71+position.z*.49;float wind=sin(u.eyeTime.w*1.2+phase)+.35*sin(u.eyeTime.w*2.6+phase*2.0);world.xz+=vec2(.06,.025)*wind*(kind==3?color.a:1.0);}
#ifdef SHADOW
 gl_Position=u.light[int(u.params.y)]*vec4(world,1);
#else
 gl_Position=u.vp*vec4(world,1);
#endif
}
