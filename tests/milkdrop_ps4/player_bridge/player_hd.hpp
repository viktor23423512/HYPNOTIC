#pragma once
static bool playerRequestedHD=true;
static bool playerHD=false;
bool milkdropPlayerRequestHD(bool hd){playerRequestedHD=hd;return !playerInitialized;}
unsigned milkdropPlayerWidth(){return width;}unsigned milkdropPlayerHeight(){return height;}
static void initializePlayerHD(GnmGpuMode mode){
 if(!playerRequestedHD)return;
 const auto savedFeedbackPS=playerFeedbackPS;const auto savedUsed=gpuUsed;FeedbackSurface saved[2]={feedback[0],feedback[1]};
 CommonSurface savedBlur[6];memcpy(savedBlur,commonBlur,sizeof(savedBlur));
 unsigned char *savedTables[2][2];memcpy(savedTables,commonTables,sizeof(savedTables));
 unsigned char *savedBlurTables[6][2];memcpy(savedBlurTables,blurTables,sizeof(savedBlurTables));
 auto savedInputs=commonInputs,savedBlurInputs=blurInputs,savedParams=feedbackParams;auto savedCommands=feedbackCommands;auto savedLabel=feedbackLabel;
 auto savedPS0=commonBlurPS[0],savedPS1=commonBlurPS[1];
 try{
  width=1920;height=1080;createFeedback(mode);createCommonRenderer(mode);
  // Descriptors captured by the shared textured-shape and final stages must
  // sample the new history, not the old 512x288 startup-control surfaces.
  for(unsigned i=0;i<2;++i){writeTexture(legacyFinalTables[i],0,feedback[i].texture,true);for(unsigned draw=0;draw<geometryBatchCapacity;++draw)writeTexture(shapeTextureTables[i][draw],0,feedback[i].texture,false);for(unsigned pick=0;pick<2;++pick)memcpy(texturedTables[i][pick],&feedback[i].texture,sizeof(GnmTexture));}
  std::string source=feedback_frag;const auto at=source.find("vec2(512.0,288.0)");if(at==std::string::npos)fail("HD FEEDBACK SHADER SIZE",-1);source.replace(at,17,"vec2(1920.0,1080.0)");
  const PsbcResourceType bindings[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER};PsbcCompileOptions options={commonTarget,PSBC_STAGE_FRAGMENT,"main",true};options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);options.resource_count=2;options.resources=bindings;
  auto output=compileText(commonTarget,PSBC_STAGE_FRAGMENT,source.c_str(),&options);playerFeedbackPS=(GnmPsShader*)prepareShader(output,false);psbc_free_output(&output);
  if(playerReadbackReady){
   for(unsigned pass=0;pass<2;++pass){auto &surface=feedback[pass];if(surface.pitch*height*4>playerReadbackCapacity)fail("HD READBACK CAPACITY",-1);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<surface.pitch;++x){auto pixel=surface.data+(y*surface.pitch+x)*4;pixel[0]=(x+pass*29)&255;pixel[1]=(y+pass*61)&255;pixel[2]=pass?197:83;pixel[3]=255;}
    const auto copy=copyPlayerReadback(surface.data,surface.pitch*height*4);if(memcmp(copy,surface.data,surface.pitch*height*4)){playerReadbackReady=false;printf("HYPNOTIC HD READBACK BYTE CHECK FAIL - DIRECT FALLBACK\n");break;}
   }
   if(playerReadbackReady)printf("HYPNOTIC HD READBACK BYTE CHECK PASS 1920 1080\n");
  }
  playerHD=true;printf("HYPNOTIC HD TARGET READY %u %u PITCH %u GPU %u/%u\n",width,height,feedback[0].pitch,(unsigned)gpuUsed,(unsigned)gpuCapacity);
 }catch(const std::exception &e){
  width=512;height=288;playerFeedbackPS=savedFeedbackPS;gpuUsed=savedUsed;feedback[0]=saved[0];feedback[1]=saved[1];memcpy(commonBlur,savedBlur,sizeof(savedBlur));memcpy(commonTables,savedTables,sizeof(savedTables));memcpy(blurTables,savedBlurTables,sizeof(savedBlurTables));commonInputs=savedInputs;blurInputs=savedBlurInputs;feedbackParams=savedParams;feedbackCommands=savedCommands;feedbackLabel=savedLabel;commonBlurPS[0]=savedPS0;commonBlurPS[1]=savedPS1;
  for(unsigned i=0;i<2;++i){writeTexture(legacyFinalTables[i],0,feedback[i].texture,true);for(unsigned draw=0;draw<geometryBatchCapacity;++draw)writeTexture(shapeTextureTables[i][draw],0,feedback[i].texture,false);for(unsigned pick=0;pick<2;++pick)memcpy(texturedTables[i][pick],&feedback[i].texture,sizeof(GnmTexture));}
  printf("HYPNOTIC HD TARGET FALLBACK %s\n",e.what());
 }
}
