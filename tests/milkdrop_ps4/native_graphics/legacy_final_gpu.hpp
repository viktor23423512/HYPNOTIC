#pragma once
#include "legacy_final_shader.hpp"
#include "legacy_final_math.hpp"
#include "hue_field.hpp"
static GnmPsShader *legacyFinalPS=nullptr;
static float *legacyFinalInputs=nullptr,legacyHueOffsets[4]={};
static unsigned char *legacyFinalTables[2]={};
static PresetMesh darkenCentreMesh;
static void activateLegacyFinal(){const unsigned mod[]={64841,53751,42661,31571};for(unsigned i=0;i<4;++i)legacyHueOffsets[i]=(nextTextureRandom()%mod[i])*.01f;}
static void createLegacyFinalRenderer(){
 legacyFinalInputs=(float *)gpuAlloc(112,16);if(!legacyFinalInputs)fail("FINAL STAGE UNIFORMS",-1);
 for(unsigned i=0;i<2;++i){legacyFinalTables[i]=(unsigned char *)gpuAlloc(128,64);if(!legacyFinalTables[i])fail("FINAL STAGE TABLE",-1);memset(legacyFinalTables[i],0,128);writeTexture(legacyFinalTables[i],0,feedback[i].texture,true);writeUniforms(legacyFinalTables[i],1,legacyFinalInputs,112);}
 const PsbcResourceType bindings[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER};PsbcCompileOptions options={commonTarget,PSBC_STAGE_FRAGMENT,"main",true};options.resource_count=2;options.resources=bindings;options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);
 auto compiled=compileText(commonTarget,PSBC_STAGE_FRAGMENT,legacy_final_frag,&options);legacyFinalPS=(GnmPsShader *)prepareShader(compiled,false);legacyFinalPS->registers.spibaryccntl=0;psbc_free_output(&compiled);
 const float center[]={0,0,0,3.f/32},edge[]={0,0,0,0};const float x[]={-.05f*.5625f,0,.05f*.5625f,0,-.05f*.5625f},y[]={0,-.05f,0,.05f,0};
 for(unsigned i=0;i<4;++i)if(!darkenCentreMesh.add(0,0,center) || !darkenCentreMesh.add(x[i],y[i],edge) || !darkenCentreMesh.add(x[i+1],y[i+1],edge))fail("DARKEN CENTRE MESH",-1);
 activateLegacyFinal();
}
static void drawLegacyFinal(unsigned source,unsigned destination,const PresetEquations::Output &v,const bool filters[4],bool unityHue=false){
 if(source>1 || destination>1 || source==destination)fail("FINAL STAGE SOURCE/TARGET",-1);
 memset(legacyFinalInputs,0,112);legacyFinalInputs[0]=v.echoZoom;legacyFinalInputs[1]=v.echoAlpha;int orientation=(int)v.echoOrient%4;legacyFinalInputs[2]=orientation%2==1;legacyFinalInputs[3]=orientation>=2;
 legacyFinalInputs[4]=v.gamma;legacyFinalInputs[5]=width;legacyFinalInputs[6]=height;for(unsigned i=0;i<4;++i)legacyFinalInputs[8+i]=filters[i];
 presetHueCorners(v.seconds,legacyHueOffsets,legacyFinalInputs+12);if(unityHue)for(unsigned i=12;i<28;++i)legacyFinalInputs[i]=1;
 drawPresetGeometry(&feedback[destination].target,pipelineQuad,false,GNM_PT_TRILIST,false,legacyFinalPS,legacyFinalTables[source],false,width,height,100);
}
static void drawDarkenCentre(unsigned destination){drawPresetGeometry(&feedback[destination].target,darkenCentreMesh,true,GNM_PT_TRILIST,false,nullptr,nullptr,false,width,height,100);}
static void checkLegacyFinal(){
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;p[0]=x*255/(width-1);p[1]=y*255/(height-1);p[2]=x<width/2?32:192;p[3]=255;}
 auto checksum=[](){uint64_t sum=0;for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width*4;++x)sum+=feedback[0].data[y*feedback[0].pitch*4+x];return sum;};const auto original=checksum();
 auto sample=[](float u,float v){float px=std::max(0.f,std::min((float)width-1,u*width-.5f)),py=std::max(0.f,std::min((float)height-1,v*height-.5f));unsigned x=(unsigned)px,y=(unsigned)py;float fx=px-x,fy=py-y;LegacyFinalMath::Color out{};
  for(unsigned c=0;c<3;++c){auto a=feedback[0].data+(y*feedback[0].pitch+x)*4;auto b=feedback[0].data+(y*feedback[0].pitch+std::min(width-1,x+1))*4;auto d=feedback[0].data+(std::min(height-1,y+1)*feedback[0].pitch+x)*4;auto e=feedback[0].data+(std::min(height-1,y+1)*feedback[0].pitch+std::min(width-1,x+1))*4;out[c]=((a[c]*(1-fx)+b[c]*fx)*(1-fy)+(d[c]*(1-fx)+e[c]*fx)*fy)/255.f;}return out;};
 bool ok=true;for(unsigned test=0;test<8;++test){PresetEquations::Output v={};v.gamma=test==0?0:test==1?.5:test==7?8:1.5;v.echoAlpha=test>=2?.4:0;v.echoOrient=test>=2?(test-2)%4:0;v.echoZoom=test%2?.75:2;bool flags[4]={test==6,test==4 || test==6,test==5 || test==6,test==7 || test==6};
  drawLegacyFinal(0,1,v,flags,true);
  for(unsigned row=0;row<2;++row)for(unsigned col=0;col<2;++col){unsigned x=col?448:64,y=row?240:48;float u,w,eu,ev;LegacyFinalMath::uv((x+.5f)/width,(y+.5f)/height,v.echoZoom,(int)v.echoOrient,u,w,eu,ev);auto expected=LegacyFinalMath::apply(sample(u,w),sample(eu,ev),{{1,1,1}},v.gamma,v.echoAlpha,flags);auto p=feedback[1].data+(y*feedback[1].pitch+x)*4;bool pass=true;for(unsigned c=0;c<3;++c)pass&=fabsf(p[c]-expected[c]*255)<=2;ok&=pass;
   printf("MILKDROP LEGACY FINAL CASE %u XY %u %u RGB %u %u %u EXPECT %.1f %.1f %.1f %s\n",test,x,y,p[0],p[1],p[2],expected[0]*255,expected[1]*255,expected[2]*255,pass?"PASS":"FAIL");}
 }
 bool retained=checksum()==original;ok&=retained;printf("MILKDROP FINAL HISTORY UNCHANGED %s\n",retained?"PASS":"FAIL");
 memset(feedback[1].data,255,feedback[1].bytes);drawDarkenCentre(1);const auto center=feedback[1].data+((height/2)*feedback[1].pitch+width/2)*4;const auto outer=feedback[1].data+((height/2)*feedback[1].pitch+width/2+32)*4;
 const float px=(width/2+.5f)/width*2-1,py=1-(height/2+.5f)/height*2;float alpha=(3.f/32)*(1-fabsf(px)/(.05f*.5625f)-fabsf(py)/.05f);bool dark=fabsf(center[0]-255*(1-alpha))<=2 && outer[0]==255;ok&=dark;printf("MILKDROP DARKEN CENTRE RGB %u EXPECT %.1f OUTER %u %s\n",center[0],255*(1-alpha),outer[0],dark?"PASS":"FAIL");
 if(!ok){note("LEGACY FINAL GPU CONTROL FAILED - SEND KLOG");hold(true);}
}
