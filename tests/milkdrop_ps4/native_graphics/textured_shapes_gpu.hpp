#pragma once
static GnmVsShader *shapeTextureVS=nullptr;
static GnmPsShader *shapeTexturePS=nullptr;
static unsigned char *shapeTextureTables[2][geometryBatchCapacity]={};
static float *shapeTextureColors[geometryBatchCapacity]={};
static const char shape_texture_vertex[]=R"glsl(#version 450
layout(set=0,binding=0,std140) uniform Geometry {vec4 data[2048];} geometry;
layout(location=0) out vec4 shapeData;
void main(){vec4 p=geometry.data[gl_VertexIndex*2];gl_Position=vec4(p.xy,0,1);shapeData=vec4(p.zw,gl_VertexIndex%3==0?1.0:0.0,1.0);}
)glsl";
static const char shape_texture_fragment[]=R"glsl(#version 450
layout(set=0,binding=0) uniform sampler2D image;
layout(set=0,binding=1,std140) uniform Colors {vec4 centre;vec4 edge;} colors;
layout(location=0) in vec4 shapeData;
layout(location=0) out vec4 result;
void main(){result=textureLod(image,shapeData.xy,0.0)*mix(colors.edge,colors.centre,shapeData.z);}
)glsl";
static void createShapeTextureRenderer(GnmGpuMode mode){
 PsbcTarget target=mode==GNM_GPU_NEO?PSBC_TARGET_PS4_NEO:PSBC_TARGET_PS4_BASE;
 const PsbcResourceType vertexBinding[]={PSBC_RESOURCE_UNIFORM_BUFFER};
 PsbcCompileOptions vo={target,PSBC_STAGE_VERTEX,"main",true};vo.resource_count=1;vo.resources=vertexBinding;vo.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);
 auto v=compileText(target,PSBC_STAGE_VERTEX,shape_texture_vertex,&vo);shapeTextureVS=(GnmVsShader *)prepareShader(v,true);psbc_free_output(&v);
 const PsbcResourceType pixelBinding[]={PSBC_RESOURCE_IMAGE_SAMPLER,PSBC_RESOURCE_UNIFORM_BUFFER};
 PsbcCompileOptions po={target,PSBC_STAGE_FRAGMENT,"main",true};po.resource_count=2;po.resources=pixelBinding;po.address32_hi=vo.address32_hi;
 auto p=compileText(target,PSBC_STAGE_FRAGMENT,shape_texture_fragment,&po);shapeTexturePS=(GnmPsShader *)prepareShader(p,false);shapeTexturePS->registers.spibaryccntl=0;psbc_free_output(&p);
 for(unsigned draw=0;draw<geometryBatchCapacity;++draw){
  shapeTextureColors[draw]=(float *)gpuAlloc(32,16);if(!shapeTextureColors[draw])fail("SHAPE COLOR UNIFORMS",-1);
  for(unsigned i=0;i<2;++i){auto &table=shapeTextureTables[i][draw];table=(unsigned char *)gpuAlloc(128,64);if(!table)fail("SHAPE TEXTURE TABLE",-1);memset(table,0,128);writeTexture(table,0,feedback[i].texture,false);writeUniforms(table,1,shapeTextureColors[draw],32);}
 }

}
static void drawTexturedShape(unsigned source,unsigned destination,const CustomShapes::Slot &shape,bool blend=true){
 const unsigned draw=geometryBatchActive?geometryBatchCount:0;
 if(source>1 || destination>1 || source==destination || draw>=geometryBatchCapacity)fail("SHAPE TEXTURE RESOURCE INDEX",-1);
 // Each batched draw owns immutable color storage until its GPU fence.
 memcpy(shapeTextureColors[draw],shape.fill.vertices[0].color,16);
 memcpy(shapeTextureColors[draw]+4,shape.fill.vertices[1].color,16);
 drawPresetGeometry(&feedback[destination].target,shape.fill,blend,GNM_PT_TRILIST,false,shapeTexturePS,shapeTextureTables[source][draw],shape.additive,width,height,100,shapeTextureVS);
}
static void checkShapeTextureRenderer(){
 printf("MILKDROP SHAPE LINK VS EXPORTS %u PS INPUTS %u\n",shapeTextureVS->numexportsemantics,shapeTexturePS->numinputsemantics);
 for(unsigned i=0;i<shapeTextureVS->numexportsemantics;++i){auto v=sceGnmVsShaderExportSemanticTable(shapeTextureVS)[i];printf("MILKDROP SHAPE VS SEMANTIC %u INDEX %u\n",v.semantic,v.outindex);}
 for(unsigned i=0;i<shapeTexturePS->numinputsemantics;++i){auto p=sceGnmPsShaderInputSemanticTable(shapeTexturePS)[i];printf("MILKDROP SHAPE PS SEMANTIC %u\n",p.semantic);}

 // Asymmetric source gradient catches swapped axes/UVs. Source and target
 // never alias; completion is fenced before the readback checks.
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=feedback[0].data+(y*feedback[0].pitch+x)*4;p[0]=x*255/(width-1);p[1]=y*255/(height-1);p[2]=64;p[3]=255;}
 CustomShapes::Slot shape;std::array<double,23> values{};values[0]=values[1]=.5;values[2]=.8;values[4]=4;values[19]=1;values[21]=2;
 for(unsigned c=5;c<13;++c)values[c]=1;
 std::string error;if(!CustomShapes::geometry(shape,values,error))fail("SHAPE CONTROL GEOMETRY",-1);
 memset(feedback[1].data,0,feedback[1].bytes);
 drawTexturedShape(0,1,shape,false);
 bool ok=true;const unsigned points[4][2]={{192,144},{320,144},{256,108},{256,180}};
 for(auto &point:points){unsigned x=point[0],y=point[1];auto p=feedback[1].data+(y*feedback[1].pitch+x)*4;
  float px=(x+.5f)/width*2-1,py=1-(y+.5f)/height*2;float u=.5f+.5f*px/.8f/2,v=.5f+.5f*py/.8f/2;
  bool pass=fabsf(p[0]-u*255)<3 && fabsf(p[1]-v*255)<3 && abs((int)p[2]-64)<2;ok&=pass;
  printf("MILKDROP SHAPE TEXTURE CONTROL %u %u RGB %u %u %u EXPECT %.2f %.2f 64 %s\n",x,y,p[0],p[1],p[2],u*255,v*255,pass?"PASS":"FAIL");
 }
 if(!ok){note("TEXTURED SHAPE GPU CHECK FAILED - SEND KLOG");hold(true);}
}

static void checkShapeViewport(){
 CustomShapes::Slot shape;std::array<double,23> values{};values[0]=values[1]=.5;values[4]=4;
 values[5]=values[9]=.25;values[6]=values[10]=.5;values[7]=values[11]=.75;values[8]=values[12]=1;
 bool ok=true;std::string error;const unsigned points[][2]={{1,1},{width-2,1},{1,height-2},{width-2,height-2},{width/2,height/2}};
 for(float radius:{10.f,-10.f}){values[2]=radius;if(!CustomShapes::geometry(shape,values,error))fail("LARGE SHAPE CONTROL GEOMETRY",-1);
  memset(feedback[1].data,0,feedback[1].bytes);drawPresetGeometry(&feedback[1].target,shape.fill,false);
  for(auto &point:points){auto p=feedback[1].data+(point[1]*feedback[1].pitch+point[0])*4;bool pass=abs((int)p[0]-64)<=2 && abs((int)p[1]-128)<=2 && abs((int)p[2]-191)<=2;ok&=pass;
   printf("MILKDROP SHAPE VIEWPORT RADIUS %.1f XY %u %u RGB %u %u %u %s\n",radius,point[0],point[1],p[0],p[1],p[2],pass?"PASS":"FAIL");}
 }
 values[0]=12;values[2]=.3;bool culled=CustomShapes::geometry(shape,values,error) && !shape.fill.count && !shape.border.count;ok&=culled;
 printf("MILKDROP SHAPE OFFSCREEN CULL %s\n",culled?"PASS":"FAIL");
 if(!ok){note("SHAPE VIEWPORT GPU CHECK FAILED - SEND KLOG");hold(true);}
}
