#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <stdexcept>
struct PresetVertex { float position[4]; float color[4]; };
static_assert(sizeof(PresetVertex)==32,"std140 geometry vertex layout");
struct PresetMesh {
    std::array<PresetVertex,1024> vertices{};
    unsigned count=0;
    bool add(float x,float y,const float color[4]) {
        if(count>=vertices.size() || !std::isfinite(x) || !std::isfinite(y))return false;
        auto &v=vertices[count++];v.position[0]=x;v.position[1]=y;v.position[2]=0;v.position[3]=1;
        for(unsigned c=0;c<4;++c)v.color[c]=std::max(0.0f,std::min(1.0f,color[c]));
        return true;
    }
};


static const unsigned width=512,height=288;struct GnmRenderTarget{};enum GnmPrimitiveType{GNM_PT_TRILIST,GNM_PT_LINESTRIP};
static bool geometryBatchActive=false;static unsigned geometryBatchCount=0,waits=0;
struct Record{PresetMesh mesh;bool blend,thick,additive;GnmPrimitiveType type;};static std::vector<Record> records;
static void fail(const char *s,int){throw std::runtime_error(s);}
static void beginGeometryBatch(const GnmRenderTarget*){assert(!geometryBatchActive);geometryBatchActive=true;geometryBatchCount=0;}
static void endGeometryBatch(){assert(geometryBatchActive);if(geometryBatchCount)++waits;geometryBatchActive=false;}
static void drawPresetGeometry(const GnmRenderTarget*,const PresetMesh &mesh,bool blend,GnmPrimitiveType type=GNM_PT_TRILIST,bool thick=false,const void *ps=nullptr,void *table=nullptr,bool additive=false){
 if(geometryBatchActive){assert(geometryBatchCount<28);++geometryBatchCount;}else ++waits;records.push_back({mesh,blend,thick,additive,type});}
#include "wave-draw-original-under-test.hpp"
#include "player_wave_pair.hpp"
int main(){GnmRenderTarget target;unsigned cases=0;
 for(unsigned n:{0u,2u,170u,171u,512u,1024u})for(unsigned m:{0u,2u,512u,1024u})for(unsigned flags=0;flags<8;++flags){
  PresetMesh a,b;const float color[]={.2f,.7f,.4f,.3f};for(unsigned i=0;i<n;++i)assert(a.add(i*.001f-.5f,sinf(i*.01f)*.4f,color));for(unsigned i=0;i<m;++i)assert(b.add(cosf(i*.01f)*.4f,i*.001f-.5f,color));
  waits=0;records.clear();playerWavePacking=false;playerDrawWavePair(&target,a,b,flags&1,flags&2,flags&4);auto expected=records;unsigned oldWaits=waits;
  waits=0;records.clear();playerWavePacking=true;playerDrawWavePair(&target,a,b,flags&1,flags&2,flags&4);
  assert(records.size()==expected.size());for(unsigned i=0;i<records.size();++i){auto &x=records[i];auto &y=expected[i];assert(x.mesh.count==y.mesh.count&&x.blend==y.blend&&x.thick==y.thick&&x.additive==y.additive&&x.type==y.type);assert(!memcmp(x.mesh.vertices.data(),y.mesh.vertices.data(),x.mesh.count*sizeof(PresetVertex)));}
  assert(!geometryBatchActive&&waits==(n||m?1u:0u)&&oldWaits==records.size());++cases;
 }
 printf("PASS %u wave-pair cases: identical draw count, every vertex/parameter/order, empty/full capacity cases, maximum14draws, one final fence\n",cases);
}
