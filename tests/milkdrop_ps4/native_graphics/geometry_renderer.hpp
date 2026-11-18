#pragma once
static GnmVsShader *geometryVS=nullptr;
static GnmPsShader *geometryPS=nullptr;
static PresetVertex *geometryVertices=nullptr;
static unsigned char *geometryTable=nullptr;
static void *geometryCommands=nullptr;
static volatile uint64_t *geometryLabel=nullptr;
// Up to four 1023-point waves, six vertices per dot, 170 dots per draw.
static constexpr unsigned geometryBatchCapacity=28;
static const size_t geometryCommandBytes=0x40000;
static PresetVertex *geometryBatchVertices=nullptr;
static unsigned char *geometryBatchTables=nullptr;
static bool geometryBatchActive=false;
static unsigned geometryBatchCount=0;
static GnmCommandBuffer geometryBatchCommand={};
static const GnmRenderTarget *geometryBatchTarget=nullptr;
static unsigned geometryLastBatchDraws=0;
static void beginGeometryBatch(const GnmRenderTarget *target) {
    if(geometryBatchActive)fail("NESTED GEOMETRY BATCH",-1);
    geometryBatchActive=true;geometryBatchCount=0;geometryBatchTarget=target;
    geometryBatchCommand=sceGnmCmdInit(geometryCommands,geometryCommandBytes,nullptr,nullptr);
}
static void finishGeometryCommands(GnmCommandBuffer &command,unsigned pollMicroseconds) {
    *geometryLabel=0;
    const uint64_t expected=0x4859504e47454f31ULL;
    sceGnmDrawCmdEventWriteEop(&command,GNM_CACHE_FLUSH_AND_INV_TS_EVENT,
        (uint64_t)(uintptr_t)geometryLabel,GNM_DATA_SEL_SEND_DATA64,expected);
    const uint32_t dwords=(uint32_t)(command.cmdptr-command.beginptr);
    if(dwords>geometryCommandBytes/4)fail("GEOMETRY COMMAND SPACE",-1);
    void *buffers[]={command.beginptr};uint32_t sizes[]={dwords*4};
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    int rc=sceGnmSubmitCommandBuffers(1,buffers,sizes,nullptr,nullptr);
    if(rc<0)fail("GEOMETRY GPU SUBMIT",rc);
    rc=sceGnmSubmitDone();if(rc<0)fail("GEOMETRY SUBMIT DONE",rc);
    const uint64_t started=sceKernelGetProcessTime();
    while(*geometryLabel!=expected && sceKernelGetProcessTime()-started<3000000)sceKernelUsleep(pollMicroseconds);
    if(*geometryLabel!=expected)fail("GEOMETRY GPU TIMEOUT",-1);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}
static void endGeometryBatch() {
    if(!geometryBatchActive)fail("GEOMETRY BATCH NOT STARTED",-1);
    geometryLastBatchDraws=geometryBatchCount;
    if(geometryBatchCount)finishGeometryCommands(geometryBatchCommand,100);
    geometryBatchActive=false;geometryBatchTarget=nullptr;
}
static const char geometry_vert[]=R"glsl(#version 450
layout(set=0,binding=0,std140) uniform Geometry { vec4 data[2048]; } geometry;
layout(location=0) out vec4 fragColor;
void main() {
    int index=gl_VertexIndex*2;
    gl_Position=geometry.data[index];
    fragColor=geometry.data[index+1];
}
)glsl";
static const char geometry_frag[]=R"glsl(#version 450
layout(location=0) in vec4 fragColor;
layout(location=0) out vec4 outColor;
void main() { outColor=fragColor; }
)glsl";

static void createGeometryShaders(PsbcTarget target)
{
    const PsbcResourceType binding[]={PSBC_RESOURCE_UNIFORM_BUFFER};
    PsbcCompileOptions options={target,PSBC_STAGE_VERTEX,"main",true};
    options.address32_hi=(uint32_t)((uintptr_t)gpuMemory>>32);
    options.resource_count=1;options.resources=binding;
    auto vert=compileText(target,PSBC_STAGE_VERTEX,geometry_vert,&options);
    auto frag=compileText(target,PSBC_STAGE_FRAGMENT,geometry_frag);
    geometryVS=(GnmVsShader *)prepareShader(vert,true);
    geometryPS=(GnmPsShader *)prepareShader(frag,false);
    geometryPS->registers.spibaryccntl=0;
    psbc_free_output(&vert);psbc_free_output(&frag);
    geometryVertices=(PresetVertex *)gpuAlloc(1024*sizeof(PresetVertex),256);
    geometryTable=(unsigned char *)gpuAlloc(64,64);
    geometryBatchVertices=(PresetVertex *)gpuAlloc(geometryBatchCapacity*1024*sizeof(PresetVertex),256);
    geometryBatchTables=(unsigned char *)gpuAlloc(geometryBatchCapacity*64,64);
    geometryCommands=gpuAlloc(geometryCommandBytes,256);
    geometryLabel=(volatile uint64_t *)gpuAlloc(8,8);
    if(!geometryVertices || !geometryTable || !geometryBatchVertices || !geometryBatchTables || !geometryCommands || !geometryLabel)fail("GEOMETRY MEMORY",-1);
    if((uintptr_t)geometryTable>>32 != (uintptr_t)gpuMemory>>32)fail("GEOMETRY ADDRESS WINDOW",-1);
    memset(geometryTable,0,64);
    GnmBuffer buffer={};sceGnmBufSetBaseAddress(&buffer,geometryVertices);
    sceGnmBufSetFormat(&buffer,GNM_FMT_R32_FLOAT);
    sceGnmBufSetMemoryType(&buffer,GNM_MEMORY_READONLY,false,false);
    buffer.stride=0;buffer.numrecords=1024*sizeof(PresetVertex);
    memcpy(geometryTable,&buffer,sizeof(buffer));
    memset(geometryBatchTables,0,geometryBatchCapacity*64);
    for(unsigned i=0;i<geometryBatchCapacity;++i){
        if((uintptr_t)(geometryBatchTables+i*64)>>32 != (uintptr_t)gpuMemory>>32)fail("BATCH ADDRESS WINDOW",-1);
        sceGnmBufSetBaseAddress(&buffer,geometryBatchVertices+i*1024);
        memcpy(geometryBatchTables+i*64,&buffer,sizeof(buffer));
    }
}
static void drawPresetGeometry(const GnmRenderTarget *target,const PresetMesh &mesh,bool blend,GnmPrimitiveType type=GNM_PT_TRILIST,bool thick=false,const GnmPsShader *pixelShader=nullptr,void *pixelTable=nullptr,bool additive=false,unsigned targetWidth=width,unsigned targetHeight=height,unsigned pollMicroseconds=1000,const GnmVsShader *vertexShader=nullptr)
{
    if(!mesh.count || mesh.count>1024 || (type==GNM_PT_TRILIST && mesh.count%3) || (type==GNM_PT_LINESTRIP && mesh.count<2))fail("GEOMETRY VERTEX COUNT",-1);
    const auto ps=pixelShader?pixelShader:geometryPS;
    const auto vs=vertexShader?vertexShader:geometryVS;
    if(geometryBatchActive && (geometryBatchCount>=geometryBatchCapacity || target!=geometryBatchTarget))fail("GEOMETRY BATCH CAPACITY OR TARGET",-1);
    auto vertices=geometryBatchActive?geometryBatchVertices+geometryBatchCount*1024:geometryVertices;
    auto table=geometryBatchActive?geometryBatchTables+geometryBatchCount*64:geometryTable;
    memcpy(vertices,mesh.vertices.data(),mesh.count*sizeof(PresetVertex));
    GnmCommandBuffer command=geometryBatchActive?geometryBatchCommand:sceGnmCmdInit(geometryCommands,geometryCommandBytes,nullptr,nullptr);
    // Reserve space before writing any packets, including the completion event.
    if((size_t)(command.cmdptr-command.beginptr)+4096>geometryCommandBytes/4)fail("GEOMETRY COMMAND RESERVE",-1);
    if(!geometryBatchActive || !geometryBatchCount){
    sceGnmDrawCmdInitDefaultHardwareState(&command);
    *command.cmdptr++=PKT3(PKT3_ACQUIRE_MEM,5,0);
    *command.cmdptr++=S_0301F0_SH_KCACHE_ACTION_ENA(1)|S_0301F0_TC_ACTION_ENA(1)|
                      S_0301F0_TCL1_ACTION_ENA(1)|S_0301F0_TC_WB_ACTION_ENA(1);
    *command.cmdptr++=0xffffffff;*command.cmdptr++=0xff;
    *command.cmdptr++=0;*command.cmdptr++=0;*command.cmdptr++=10;
    sceGnmDrawCmdWaitGraphicsWrite(&command,GNM_ACQUIRE_TARGET_CB0);
    }
    contextRegister(&command,R_028808_CB_COLOR_CONTROL,S_028808_MODE(V_028808_CB_NORMAL)|S_028808_ROP3(0xcc));
    contextRegister(&command,R_028204_PA_SC_WINDOW_SCISSOR_TL,0x80000000);
    contextRegister(&command,R_028208_PA_SC_WINDOW_SCISSOR_BR,targetWidth|(targetHeight<<16));
    contextRegister(&command,R_028240_PA_SC_GENERIC_SCISSOR_TL,0x80000000);
    contextRegister(&command,R_028244_PA_SC_GENERIC_SCISSOR_BR,targetWidth|(targetHeight<<16));
    contextRegister(&command,R_028C38_PA_SC_AA_MASK_X0Y0_X1Y0,0xffffffff);
    contextRegister(&command,R_028C3C_PA_SC_AA_MASK_X0Y1_X1Y1,0xffffffff);
    GnmDbRenderControl db={};sceGnmDrawCmdSetDbRenderControl(&command,&db);
    GnmDepthStencilControl depth={};sceGnmDrawCmdSetDepthStencilControl(&command,&depth);
    GnmPrimitiveSetup primitive={};primitive.cullmode=GNM_CULL_NONE;
    primitive.frontface=GNM_FACE_CCW;primitive.frontmode=primitive.backmode=GNM_FILL_SOLID;
    primitive.provokemode=GNM_PROVOKINGVTX_FIRST;
    sceGnmDrawCmdSetPrimitiveSetup(&command,&primitive);
    sceGnmDrawCmdSetRenderTarget(&command,0,target);
    sceGnmDrawCmdSetRenderTargetMask(&command,0xf);
    GnmSetViewportInfo viewport={};viewport.dmin=0;viewport.dmax=1;
    viewport.scale[0]=targetWidth/2.0f;viewport.scale[1]=-(float)targetHeight/2.0f;viewport.scale[2]=0.5f;
    viewport.offset[0]=targetWidth/2.0f;viewport.offset[1]=targetHeight/2.0f;viewport.offset[2]=0.5f;
    sceGnmDrawCmdSetViewport(&command,0,&viewport);
    GnmViewportTransformControl transform={};
    transform.scalex=transform.offsetx=transform.scaley=transform.offsety=transform.scalez=transform.offsetz=transform.invertw=true;
    sceGnmDrawCmdSetViewportTransformControl(&command,&transform);
    sceGnmDrawCmdSetScreenScissor(&command,0,0,targetWidth,targetHeight);
    sceGnmDrawCmdSetHwScreenOffset(&command,0,0);
    sceGnmDrawCmdSetGuardBands(&command,1,1,1,1);
    GnmBlendControl blending={};blending.blendenabled=blend;
    blending.colorfunc=GNM_COMB_DST_PLUS_SRC;blending.colorsrcmult=GNM_BLEND_SRC_ALPHA;
    blending.colordstmult=additive?GNM_BLEND_ONE:GNM_BLEND_ONE_MINUS_SRC_ALPHA;
    sceGnmDrawCmdSetBlendControl(&command,0,&blending);
    sceGnmDrawCmdSetVsShader(&command,&vs->registers,0);
    sceGnmDrawCmdSetPsShader(&command,&ps->registers);
    sceGnmDrawCmdSetPsInputUsage(&command,sceGnmVsShaderExportSemanticTable(vs),vs->numexportsemantics,
        sceGnmPsShaderInputSemanticTable(ps),ps->numinputsemantics);
    const auto slots=sceGnmVsShaderInputUsageSlotTable(vs);
    unsigned matches=0;
    for(unsigned i=0;i<vs->common.numinputusageslots;++i) {
        if(slots[i].usagetype!=GNM_SHINPUTUSAGE_PTR_INDIRECTRESOURCETABLE)continue;
        if(slots[i].startregister>=16)fail("GEOMETRY USER DATA SLOT",-1);
        *command.cmdptr++=PKT3(PKT3_SET_SH_REG,1,0);
        *command.cmdptr++=(R_00B130_SPI_SHADER_USER_DATA_VS_0-0xB000)/4+slots[i].startregister;
        *command.cmdptr++=(uint32_t)(uintptr_t)table;
        ++matches;
    }
    if(matches!=1)fail("GEOMETRY RESOURCE TABLE",-1);
    if(pixelShader){if(!pixelTable)fail("GEOMETRY PIXEL RESOURCES",-1);bindResourceTable(&command,ps,pixelTable,false);}
    sceGnmDrawCmdSetNumInstances(&command,1);
    sceGnmDrawCmdSetPrimitiveType(&command,type);
    if(type==GNM_PT_LINESTRIP)contextRegister(&command,R_028A08_PA_SU_LINE_CNTL,S_028A08_WIDTH(8));
    for(unsigned pass=0;pass<(thick?4u:1u);++pass) {
        viewport.offset[0]=targetWidth/2.0f+((pass==1 || pass==2)?1.0f:0.0f);
        viewport.offset[1]=targetHeight/2.0f+((pass==2 || pass==3)?1.0f:0.0f);
        sceGnmDrawCmdSetViewport(&command,0,&viewport);
        sceGnmDrawCmdDrawIndexAuto(&command,mesh.count);
    }
    if(geometryBatchActive){geometryBatchCommand=command;++geometryBatchCount;}
    else finishGeometryCommands(command,pollMicroseconds);
}
static void geometryControl(GnmRenderTarget *target,unsigned char *data,unsigned pitch,size_t bytes)
{
    memset(data,0,bytes);
    PresetMesh mesh;
    const float red[4]={1,0,0,1},green[4]={0,1,0,1},blue[4]={0,0,1,1};
    mesh.add(0,-0.5f,red);mesh.add(0.5f,0.5f,green);mesh.add(-0.5f,0.5f,blue);
    notify("CHECKING GPU VERTEX DATA DRAW");
    drawPresetGeometry(target,mesh,false);
    const auto center=data+((height/2)*pitch+width/2)*4;
    unsigned colored=0;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const auto p=data+(y*pitch+x)*4;
        if(p[0]>8 || p[1]>8 || p[2]>8)++colored;
    }
    const bool verified=colored>18000 && colored<19000 && center[0]>120 && center[0]<136 &&
        center[1]>55 && center[1]<73 && center[2]>55 && center[2]<73 && data[0]==0;
    note("GPU VERTEX DATA %s - PIXELS %u CENTER %u %u %u",verified?"PASS":"FAIL",colored,center[0],center[1],center[2]);
    if(!verified){pixels=data;pixelPitch=pitch;hold(true);}
    memset(data,0,bytes);
    PresetMesh line;const float white[4]={1,1,1,1};
    line.add(-0.5f,0,white);line.add(0.5f,0,white);
    drawPresetGeometry(target,line,false,GNM_PT_LINESTRIP);
    unsigned whitePixels=0;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const auto p=data+(y*pitch+x)*4;if(p[0]>240 && p[1]>240 && p[2]>240)++whitePixels;
    }
    note("GPU LINE DATA %s - PIXELS %u",whitePixels>=250 && whitePixels<=270?"PASS":"FAIL",whitePixels);
    if(whitePixels<250 || whitePixels>270){pixels=data;pixelPitch=pitch;hold(true);}
    // Two 50%-alpha overlays must add, not overwrite.
    memset(data,0,bytes);PresetMesh rectangle;const float tint[4]={.4f,.2f,.1f,.5f};
    rectangle.add(-.25f,-.25f,tint);rectangle.add(.25f,-.25f,tint);rectangle.add(-.25f,.25f,tint);
    rectangle.add(.25f,-.25f,tint);rectangle.add(.25f,.25f,tint);rectangle.add(-.25f,.25f,tint);
    drawPresetGeometry(target,rectangle,true,GNM_PT_TRILIST,false,nullptr,nullptr,true);
    drawPresetGeometry(target,rectangle,true,GNM_PT_TRILIST,false,nullptr,nullptr,true);
    const auto blended=data+((height/2)*pitch+width/2)*4;
    const bool additive=abs((int)blended[0]-102)<=3 && abs((int)blended[1]-51)<=3 && abs((int)blended[2]-26)<=3;
    note("GPU ADDITIVE SHAPE CONTROL %s - RGB %u %u %u",additive?"PASS":"FAIL",blended[0],blended[1],blended[2]);
    if(!additive){pixels=data;pixelPitch=pitch;hold(true);}
}

static void drawStandardWave(const GnmRenderTarget *target,const PresetMesh &mesh,bool dots,bool thick,bool additive,float dotPixels=2){
    if(!mesh.count)return;
    if(!dots){drawPresetGeometry(target,mesh,true,GNM_PT_LINESTRIP,thick,nullptr,nullptr,additive);return;}
    static PresetMesh batch;batch.count=0;
    for(unsigned i=0;i<mesh.count;++i){
        const auto &v=mesh.vertices[i];const float x=v.position[0]-dotPixels/width,y=v.position[1]-dotPixels/height,dx=2*dotPixels/width,dy=2*dotPixels/height;
        const float xy[6][2]={{x,y},{x+dx,y},{x,y+dy},{x+dx,y},{x+dx,y+dy},{x,y+dy}};
        for(auto &p:xy)if(!batch.add(p[0],p[1],v.color))fail("DOT VERTEX SPACE",-1);
        if(batch.count==1020 || i+1==mesh.count){drawPresetGeometry(target,batch,true,GNM_PT_TRILIST,false,nullptr,nullptr,additive);batch.count=0;}
    }
}

static void dotControl(GnmRenderTarget *target,unsigned char *data,unsigned pitch,size_t bytes){
    memset(data,0,bytes);PresetMesh dots;const float white[4]={1,1,1,1};
    dots.add(0,0,white);dots.add(.5f,.25f,white);
    drawStandardWave(target,dots,true,false,false);
    unsigned count=0;for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto p=data+(y*pitch+x)*4;if(p[0]>240 && p[1]>240 && p[2]>240)++count;}
    note("GPU WAVE DOT CONTROL %s - PIXELS %u",count==8?"PASS":"FAIL",count);
    if(count!=8){pixels=data;pixelPitch=pitch;hold(true);}
}

static void vectorLineControl(GnmRenderTarget *target,unsigned char *data,unsigned pitch,size_t bytes){
 memset(data,0,bytes);PresetMesh lines;const float white[4]={1,1,1,1};lines.add(-.5f,-.25f,white);lines.add(.5f,-.25f,white);lines.add(-.5f,.25f,white);lines.add(.5f,.25f,white);
 drawPresetGeometry(target,lines,true,GNM_PT_LINELIST,false,nullptr,nullptr,false,width,height,100);
 unsigned count=0;for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto p=data+(y*pitch+x)*4;if(p[0]>240 && p[1]>240 && p[2]>240)++count;}
 const auto centre=data+((height/2)*pitch+width/2)*4;bool pass=count>=500 && count<=530 && centre[0]==0;
 note("GPU VECTOR LINE LIST %s - PIXELS %u",pass?"PASS":"FAIL",count);if(!pass){pixels=data;pixelPitch=pitch;hold(true);}
}

// Compare full readback against the original serial renderer. Distinct colours
// and overlapping translucent dots detect overwritten upload buffers and order.
static void geometryBatchControl(GnmRenderTarget *target,unsigned char *data,unsigned pitch,size_t bytes){
    auto reference=new unsigned char[bytes];
    static PresetMesh mesh;
    auto scene=[&](){
        for(unsigned slot=0;slot<4;++slot){
            mesh.count=0;
            for(unsigned i=0;i<1023;++i){
                const float t=i/1022.0f;
                const float colour[4]={slot==0?1.f:.2f,slot==1?1.f:.3f,slot==2?1.f:.4f,.35f};
                mesh.add((t-.5f)*1.5f,.35f*sinf(t*12.f)+slot*.03f,colour);
            }
            drawStandardWave(target,mesh,true,false,slot%2,slot%2?2:1);
        }
    };
    memset(data,0,bytes);scene();memcpy(reference,data,bytes);
    memset(data,0,bytes);beginGeometryBatch(target);scene();endGeometryBatch();
    unsigned differences=0;for(size_t i=0;i<bytes;++i)if(reference[i]!=data[i])++differences;
    note("GPU POINT BATCH EQUIVALENCE %s - DIFFERENCES %u DRAWS %u SUBMITS 1",differences?"FAIL":"PASS",differences,geometryLastBatchDraws);
    delete[] reference;
    if(differences){pixels=data;pixelPitch=pitch;hold(true);}
    // Also cover line strips with shifted thick passes and changing blend state.
    auto lines=[&](){for(unsigned slot=0;slot<4;++slot){
        mesh.count=0;for(unsigned i=0;i<96;++i){const float t=i/95.f;const float c[4]={t,slot*.25f,1-t,.4f};mesh.add(t*1.6f-.8f,.4f*sinf(t*15)+slot*.02f,c);}
        drawStandardWave(target,mesh,false,slot%2,slot>=2);
    }};
    reference=new unsigned char[bytes];memset(data,0,bytes);lines();memcpy(reference,data,bytes);
    memset(data,0,bytes);beginGeometryBatch(target);lines();endGeometryBatch();
    differences=0;for(size_t i=0;i<bytes;++i)if(reference[i]!=data[i])++differences;
    delete[] reference;
    note("GPU LINE BATCH EQUIVALENCE %s - DIFFERENCES %u",differences?"FAIL":"PASS",differences);
    if(differences){pixels=data;pixelPitch=pitch;hold(true);}
}
