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

