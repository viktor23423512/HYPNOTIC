#pragma once
// Coalesce complete triangles only. Vertex bytes, primitive order, blending
// and barriers around textured fills/outlines are preserved exactly.
struct PlayerTriangleBatch {
 PresetMesh mesh;bool additive=false;
 template<class Emit> void flush(Emit emit){if(mesh.count){emit(mesh,additive);mesh.count=0;}}
 template<class Emit> void append(const PresetMesh &input,bool blend,Emit emit){
  if(input.count>1024 || input.count%3)fail("SHAPE TRIANGLE COUNT",-1);
  if(mesh.count && additive!=blend)flush(emit);additive=blend;
  unsigned offset=0;
  while(offset<input.count){unsigned take=std::min(input.count-offset,1023u-mesh.count);
   memcpy(mesh.vertices.data()+mesh.count,input.vertices.data()+offset,take*sizeof(PresetVertex));mesh.count+=take;offset+=take;
   if(mesh.count==1023)flush(emit);
  }
 }
};
