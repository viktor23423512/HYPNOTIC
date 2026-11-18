#pragma once
static const char legacy_final_frag[]=R"glsl(#version 450
layout(set=0,binding=0) uniform sampler2D image;
layout(set=0,binding=1,std140) uniform Final {vec4 echo;vec4 gainSize;vec4 filters;vec4 corners[4];} p;
layout(location=0) in vec4 fragUV;
layout(location=0) out vec4 outColor;
vec3 q8(vec3 x){return floor(clamp(x,vec3(0),vec3(1))*255.0+0.5)/255.0;}
vec3 gammaAdd(vec3 dst,vec3 add,float gain){
 dst=q8(dst+clamp(add*clamp(gain-0.0,0.0,1.0),vec3(0),vec3(1)));
 dst=q8(dst+clamp(add*clamp(gain-1.0,0.0,1.0),vec3(0),vec3(1)));
 dst=q8(dst+clamp(add*clamp(gain-2.0,0.0,1.0),vec3(0),vec3(1)));
 dst=q8(dst+clamp(add*clamp(gain-3.0,0.0,1.0),vec3(0),vec3(1)));
 dst=q8(dst+clamp(add*clamp(gain-4.0,0.0,1.0),vec3(0),vec3(1)));
 dst=q8(dst+clamp(add*clamp(gain-5.0,0.0,1.0),vec3(0),vec3(1)));
 dst=q8(dst+clamp(add*clamp(gain-6.0,0.0,1.0),vec3(0),vec3(1)));
 dst=q8(dst+clamp(add*clamp(gain-7.0,0.0,1.0),vec3(0),vec3(1)));
return dst;}
void main(){
 vec2 uv=(fragUV.xy*p.gainSize.yz+vec2(.5))/(p.gainSize.yz+vec2(1));
 vec3 shade=uv.x+uv.y<=1.0?p.corners[0].rgb*(1-uv.x-uv.y)+p.corners[1].rgb*uv.x+p.corners[2].rgb*uv.y:
   p.corners[1].rgb*(1-uv.y)+p.corners[2].rgb*(1-uv.x)+p.corners[3].rgb*(uv.x+uv.y-1);
 vec3 a=textureLod(image,uv,0).rgb*shade;vec3 c=vec3(0);
 if(p.echo.y>.001){vec2 e=(uv-.5)/p.echo.x+.5;if(p.echo.z>.5)e.x=1-e.x;if(p.echo.w>.5)e.y=1-e.y;
  c=gammaAdd(c,a*(1-p.echo.y),max(1.0,p.gainSize.x));c=gammaAdd(c,textureLod(image,e,0).rgb*shade*p.echo.y,max(1.0,p.gainSize.x));
 }else c=gammaAdd(c,a,p.gainSize.x);
 if(p.filters.x>.5){c=q8(1-c);c=q8(c*c);c=q8(1-c);}
 if(p.filters.y>.5)c=q8(c*c);
 if(p.filters.z>.5){c=q8(c*(1-c));c=q8(c+c);}
 if(p.filters.w>.5)c=q8(1-c);
 outColor=vec4(c,1);
}
)glsl";
