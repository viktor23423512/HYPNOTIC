#pragma once
static void checkPlayerWavePacking(){
 static PresetMesh first,second;first.count=second.count=0;
 for(unsigned i=0;i<512;++i){float t=i/511.f;const float color[]={.2f+.6f*t,.8f-.4f*t,.4f,.35f};
  first.add(-.9f+1.8f*t,.4f*sinf(t*19),color);second.add(.4f*cosf(t*15),-.9f+1.8f*t,color);
 }
 static unsigned char expected[512*288*4];bool equal=true;
 for(unsigned mode=0;mode<4;++mode){const bool dots=mode&1,thick=mode&2,additive=mode&2;
  memset(feedback[1].data,0,feedback[1].bytes);playerWavePacking=false;playerDrawWavePair(&feedback[1].target,first,second,dots,thick,additive);
  for(unsigned y=0;y<height;++y)memcpy(expected+y*width*4,feedback[1].data+y*feedback[1].pitch*4,width*4);
  memset(feedback[1].data,0,feedback[1].bytes);playerWavePacking=true;playerDrawWavePair(&feedback[1].target,first,second,dots,thick,additive);
  bool pass=true;for(unsigned y=0;y<height;++y)pass&=memcmp(expected+y*width*4,feedback[1].data+y*feedback[1].pitch*4,width*4)==0;
  equal&=pass;printf("HYPNOTIC MILKDROP WAVE PACKING MODE %u BYTE CHECK %s\n",mode,pass?"PASS":"FAIL");
 }
 playerWavePacking=equal;printf("HYPNOTIC MILKDROP WAVE PACKING %s\n",equal?"ENABLED":"ORIGINAL FALLBACK");
}
