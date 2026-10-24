#version 450
#extension GL_GOOGLE_include_directive:require
#include "uniform.glsl"
layout(location=3)in vec2 texcoord;layout(location=4)in vec4 tint;layout(location=0)out vec4 outputColor;
    // Integer mixing keeps the fractional noise alive on FP16 mobile GPUs.
    float hash(vec2 p) {
        ivec2 cell=ivec2(p);
        uvec2 q=uvec2(cell);
        uint bits=q.x*0x9e3779b9u ^ q.y*0x85ebca6bu;
        bits ^= bits >> 16u;
        bits *= 0x7feb352du;
        bits ^= bits >> 15u;
        bits *= 0x846ca68bu;
        bits ^= bits >> 16u;
        return float(bits >> 8u)*(1.0/16777216.0);
    }
    float noise(vec2 p) {
        vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);
        return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)),f.x),f.y);
    }
    float turbulence(vec2 p) {
        return noise(p)*0.57+noise(p*2.03+7.0)*0.28+noise(p*4.11)*0.15;
    }
    void main() {

        vec2 uv=texcoord,p=uv*2.0-1.0;
        vec4 particleTint=tint;
        if(particleTint.a<0.003) discard;
        float mask=0.0;
        float emission=0.0;
        vec3 color=particleTint.rgb;
        if((int(u.params.x)-5)==0) {
            float taper=1.0-uv.y*0.65;
            float envelope=1.0-smoothstep(0.15,1.0,length(vec2(p.x/taper,p.y)));
            if(envelope*particleTint.a<0.003) discard;
            float n=turbulence(uv*vec2(4,5)-vec2(0,u.eyeTime.w*2.8));
            mask=envelope*smoothstep(0.15,0.65,n);
            emission=48000.0;
        } else if((int(u.params.x)-5)==1) {
            float envelope=1.0-smoothstep(0.35,1.0,length(p));
            if(envelope*particleTint.a<0.003) discard;
            float n=turbulence(uv*5.0-vec2(u.eyeTime.w*0.6));
            mask=envelope*smoothstep(0.1,0.7,n);
            color*=mix(0.35,1.0,n);
            emission=60000.0;
        } else if((int(u.params.x)-5)==2) {
            mask=1.0-smoothstep(0.15,1.0,length(p));
        } else if((int(u.params.x)-5)==3) {
            float core=1.0-smoothstep(0.03,0.18,abs(p.x));
            mask=exp(-abs(p.x)*4.0);
            color=mix(color,vec3(0.92,0.96,1.0),core);
            emission=280000.0*(0.3+core*0.7);
        } else {
            float envelope=1.0-smoothstep(0.25,1.0,length(p));
            if(envelope*particleTint.a<0.003) discard;
            float n=turbulence(uv*4.0+vec2(u.eyeTime.w*0.12,-u.eyeTime.w*0.2));
            mask=envelope*smoothstep(0.12,0.7,n);
            color*=0.5+0.9*n+0.25*uv.y;
        }
        float alpha=clamp(mask*particleTint.a,0.0,1.0);
        if(alpha<0.003) discard;
        vec3 rgb=emission>0.0?color*(emission/38400.0):color;
        outputColor=vec4(rgb,alpha);
    }
