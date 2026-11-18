#include <algorithm>
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <stdexcept>
struct PresetVertex{float position[4],color[4];};
struct PresetMesh{std::array<PresetVertex,1024> vertices{};unsigned count=0;};
static void fail(const char*s,int){throw std::runtime_error(s);}
#include "player_triangle_batch.hpp"
struct Piece{std::vector<PresetVertex> vertices;bool additive;};
int main(){PlayerTriangleBatch batch;std::vector<Piece> original,packed;unsigned draws=0;
 auto emit=[&](const PresetMesh &mesh,bool additive){assert(mesh.count>0&&mesh.count<=1023&&mesh.count%3==0);packed.push_back({std::vector<PresetVertex>(mesh.vertices.begin(),mesh.vertices.begin()+mesh.count),additive});++draws;};
 unsigned id=0;for(unsigned i=0;i<2000;++i){PresetMesh mesh;mesh.count=(3+i%98)*3;bool additive=(i/97)&1;
  for(unsigned j=0;j<mesh.count;++j){for(unsigned k=0;k<4;++k){mesh.vertices[j].position[k]=(id+k)*.001f;mesh.vertices[j].color[k]=(id+k)%256*.003f;}++id;}
  original.push_back({std::vector<PresetVertex>(mesh.vertices.begin(),mesh.vertices.begin()+mesh.count),additive});batch.append(mesh,additive,emit);
  if(i%31==0)batch.flush(emit); // textured/outline barrier
 }
 batch.flush(emit);assert(batch.mesh.count==0&&draws<500);
 std::vector<PresetVertex> a,b;std::vector<bool> ab,bb;
 for(auto &p:original){a.insert(a.end(),p.vertices.begin(),p.vertices.end());for(unsigned i=0;i<p.vertices.size();++i)ab.push_back(p.additive);}
 for(auto &p:packed){b.insert(b.end(),p.vertices.begin(),p.vertices.end());for(unsigned i=0;i<p.vertices.size();++i)bb.push_back(p.additive);}
 assert(a.size()==b.size()&&!memcmp(a.data(),b.data(),a.size()*sizeof(PresetVertex))&&ab==bb);
 PresetMesh invalid;invalid.count=4;bool threw=false;try{batch.append(invalid,false,emit);}catch(const std::runtime_error&){threw=true;}assert(threw);
 printf("PASS 2000 shape meshes -> %u draws, all %u vertex bytes/order/blend preserved, barriers and capacity/triangle validation\n",draws,(unsigned)(a.size()*sizeof(PresetVertex)));
}
