#pragma once
// Adapted pinned projectM 4.1.8 Blur1/Blur2 GLSL kernels (LGPL).
static const char common_blur_0[]=R"glsl(#version 450

layout(location=0) in vec4 fragment_data;
#define fragment_texture fragment_data.xy

layout(set=0,binding=0) uniform sampler2D texture_sampler;
layout(set=0,binding=1,std140) uniform BlurInputs {vec4 _c0;vec4 _c1;vec4 _c2;vec4 _c3;};




layout(location=0) out vec4 color;

void main(){
    // LONG HORIZ. PASS 1:
    #define srctexsize _c0
    #define w1 _c1.x
    #define w2 _c1.y
    #define w3 _c1.z
    #define w4 _c1.w
    #define d1 _c2.x
    #define d2 _c2.y
    #define d3 _c2.z
    #define d4 _c2.w
    #define fscale _c3.x
    #define fbias  _c3.y
    #define w_div  _c3.z

    // note: if you just take one sample at exactly uv.xy, you get an avg of 4
    // pixels.

    // + moves blur UP, LEFT by 1-pixel increments
    vec2 uv2 = fragment_texture.xy + srctexsize.zw*vec2(1.0,1.0);

    vec3 blur =
    (texture(texture_sampler, uv2 + vec2(d1 * srctexsize.z, 0)).xyz +
    texture(texture_sampler, uv2 + vec2(-d1 * srctexsize.z, 0)).xyz) *
    w1 +
    (texture(texture_sampler, uv2 + vec2(d2 * srctexsize.z, 0)).xyz +
    texture(texture_sampler, uv2 + vec2(-d2 * srctexsize.z, 0)).xyz) *
    w2 +
    (texture(texture_sampler, uv2 + vec2(d3 * srctexsize.z, 0)).xyz +
    texture(texture_sampler, uv2 + vec2(-d3 * srctexsize.z, 0)).xyz) *
    w3 +
    (texture(texture_sampler, uv2 + vec2(d4 * srctexsize.z, 0)).xyz +
    texture(texture_sampler, uv2 + vec2(-d4 * srctexsize.z, 0)).xyz) *
    w4;

    blur.xyz *= w_div;

    blur.xyz = blur.xyz*fscale + fbias;

    color.xyz = blur;
    color.w   = 1.0;
}
)glsl";
static const char common_blur_1[]=R"glsl(#version 450

layout(location=0) in vec4 fragment_data;
#define fragment_texture fragment_data.xy

layout(set=0,binding=0) uniform sampler2D texture_sampler;
layout(set=0,binding=1,std140) uniform BlurInputs {vec4 _c0;vec4 _c1;vec4 _c2;vec4 _c3;};



layout(location=0) out vec4 color;

void main(){
    // SHORT VERTICAL PASS 2:
    #define srctexsize _c0
    #define w1 _c1.x
    #define w2 _c1.y
    #define d1 _c1.z
    #define d2 _c1.w
    #define edge_darken_c1 _c2.y
    #define edge_darken_c2 _c2.z
    #define edge_darken_c3 _c2.w
    #define w_div   _c2.x

    // note: if you just take one sample at exactly uv.xy, you get an avg of 4
    // pixels.

    // + moves blur UP, LEFT by TWO-pixel increments! (since texture is 1/2 the
    // size of blur1_ps)
    vec2 uv2 = fragment_texture.xy + srctexsize.zw*vec2(0,0);

    vec3 blur =
    (texture(texture_sampler, uv2 + vec2(0, d1 * srctexsize.w)).xyz +
    texture(texture_sampler, uv2 + vec2(0, -d1 * srctexsize.w)).xyz) *
    w1 +
    (texture(texture_sampler, uv2 + vec2(0, d2 * srctexsize.w)).xyz +
    texture(texture_sampler, uv2 + vec2(0, -d2 * srctexsize.w)).xyz) *
    w2;
    blur.xyz *= w_div;

    // tone it down at the edges (only happens on 1st X pass!)
    float t = min(min(fragment_texture.x, fragment_texture.y),
    1.0 - max(fragment_texture.x, fragment_texture.y));
    t = sqrt(t);
    t = edge_darken_c1 + edge_darken_c2 * clamp(t * edge_darken_c3, 0.0, 1.0);
    blur.xyz *= t;

    color.xyz = blur;
    color.w = 1.0;
})glsl";
