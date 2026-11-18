static void setCommonInputs(const PresetEquations::Output &v){
    for(unsigned source=0;source<2;++source)for(unsigned pick=0;pick<2;++pick)writeTexture(commonTables[source][pick],0,feedback[source].texture,v.textureWrap<=.0001);
    if(!commonBlurConfig.set(v.blurMin,v.blurMax,v.blurEdge))fail("INVALID BLUR CONFIG",-1);
    memset(commonInputs,0,27*16);commonInputs[0]=512;commonInputs[1]=288;commonInputs[2]=1.f/512;commonInputs[3]=1.f/288;
    for(unsigned i=0;i<3;++i){commonInputs[4+i]=v.audio[i];commonInputs[8+i]=v.audio[i+3];}
    commonInputs[7]=(v.audio[0]+v.audio[1]+v.audio[2])/3;commonInputs[11]=(v.audio[3]+v.audio[4]+v.audio[5])/3;
    commonInputs[12]=v.seconds;commonInputs[13]=v.fps;commonInputs[14]=v.frame;commonInputs[15]=0;
    commonInputs[16]=1;commonInputs[17]=.5625f;commonInputs[18]=1;commonInputs[19]=1.f/.5625f;
    for(unsigned i=0;i<4;++i){commonInputs[20+i]=(nextTextureRandom()>>8)*(1.f/16777216);commonInputs[24+i]=commonPresetRandom[i];}
    for(unsigned i=0;i<32;++i)commonInputs[28+i]=v.q[i];
    shaderRoamingInputs((float)v.seconds,commonInputs+60);
    presetHueCorners(v.seconds,commonHueOffsets,commonInputs+92);
    commonInputs[76]=commonInputs[77]=256;commonInputs[78]=commonInputs[79]=1.f/256;
    commonInputs[84]=commonBlurConfig.max[0]-commonBlurConfig.min[0];commonInputs[85]=commonBlurConfig.min[0];commonInputs[86]=commonBlurConfig.max[1]-commonBlurConfig.min[1];commonInputs[87]=commonBlurConfig.min[1];commonInputs[88]=commonBlurConfig.max[2]-commonBlurConfig.min[2];commonInputs[89]=commonBlurConfig.min[2];commonInputs[90]=presetSurfaceDecay(v.decay);
    const auto &image=imageTextures[imageChoice];commonInputs[80]=image.width;commonInputs[81]=image.height;commonInputs[82]=1.f/image.width;commonInputs[83]=1.f/image.height;
}
