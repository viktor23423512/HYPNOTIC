#pragma once
#include "player_triangle_batch.hpp"
static bool playerShapePacking=true;
static PlayerTriangleBatch playerTriangles;
static void playerDrawShapes(CustomShapes *shapes,unsigned source){
 auto target=&feedback[source^1].target;
 auto room=[&](){if(geometryBatchCount==geometryBatchCapacity){endGeometryBatch();beginGeometryBatch(target);}};
 auto emit=[&](const PresetMesh &mesh,bool additive){room();drawPresetGeometry(target,mesh,true,GNM_PT_TRILIST,false,nullptr,nullptr,additive);};
 playerTriangles.mesh.count=0;beginGeometryBatch(target);
 for(auto &shape:shapes->slots)if(shape.enabled)for(const auto &copy:shape.copies){
  std::string error;if(!CustomShapes::geometry(shape,copy,error))fail("VALIDATED SHAPE GEOMETRY",-1);
  if(shape.fill.count){
   if(copy[19]!=0){playerTriangles.flush(emit);room();drawTexturedShape(source,source^1,shape);}
   else if(playerShapePacking)playerTriangles.append(shape.fill,shape.additive,emit);
   else emit(shape.fill,shape.additive);
  }
  if(shape.border.count){playerTriangles.flush(emit);room();drawPresetGeometry(target,shape.border,true,GNM_PT_LINESTRIP,shape.thick,nullptr,nullptr,shape.additive);}
 }
 playerTriangles.flush(emit);endGeometryBatch();
}
static void checkPlayerShapePacking(){
 // Compare overlapping alpha/additive shapes, changing triangle counts,
 // outline barriers and a textured barrier against the original draw path.
 CustomShapes fixture;fixture.slots[0].enabled=true;fixture.slots[0].copies.resize(100);
 for(unsigned i=0;i<100;++i){auto &v=fixture.slots[0].copies[i];v={};v[0]=.3+(i%7)*.06;v[1]=.3+(i%5)*.08;v[2]=.12;v[3]=i*.1;v[4]=3+i%15;
  v[5]=.2+i*.003;v[6]=.7;v[7]=.3;v[8]=.35;v[9]=.8;v[10]=.1;v[11]=.6;v[12]=.2;
  v[13]=.6;v[14]=.8;v[15]=.2;v[16]=i%17==0?.4:0;v[17]=(i/25)&1;v[18]=i&1;v[19]=i%31==0;v[21]=1;
 }
 // Textured shapes read source0, which must be unchanged in both runs.
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;p[0]=x%256;p[1]=y%256;p[2]=97;p[3]=255;}
 static unsigned char expected[512*288*4];
 memset(feedback[1].data,0,feedback[1].bytes);playerShapePacking=false;playerDrawShapes(&fixture,0);
 for(unsigned y=0;y<height;++y)memcpy(expected+y*width*4,feedback[1].data+y*feedback[1].pitch*4,width*4);
 memset(feedback[1].data,0,feedback[1].bytes);playerShapePacking=true;playerDrawShapes(&fixture,0);
 bool equal=true;for(unsigned y=0;y<height;++y)equal&=memcmp(expected+y*width*4,feedback[1].data+y*feedback[1].pitch*4,width*4)==0;
 playerShapePacking=equal;printf("HYPNOTIC MILKDROP SHAPE PACKING BYTE CHECK %s - %s\n",equal?"PASS":"FAIL",equal?"PACKED":"ORIGINAL FALLBACK");
}
