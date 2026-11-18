#pragma once
static bool playerWavePacking=true;
static void playerDrawWavePair(const GnmRenderTarget *target,const PresetMesh &first,const PresetMesh &second,bool dots,bool thick,bool additive){
 const bool batch=playerWavePacking && (first.count || second.count);
 // Each mesh has <=1024points. Two dotted meshes produce <=14draws,
 // below the existing28draw capacity. Every draw has distinct GPU storage.
 if(batch)beginGeometryBatch(target);
 drawStandardWave(target,first,dots,thick,additive);
 drawStandardWave(target,second,dots,thick,additive);
 if(batch)endGeometryBatch();
}
