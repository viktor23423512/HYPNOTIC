#include <cstdio>
#include <initializer_list>
#include <cassert>
#include <cmath>
#include <cstring>
#define HYPNOTIC_GPU_MESH_TEST
struct Color{unsigned char r,g,b;};
static bool preview=false;static unsigned calls=0,vertices=0;
struct GpuScene {struct Vertex{float x,y,u,v;};void drawSolidMesh(const Vertex *v,unsigned n,Color c,float opacity){++calls;vertices+=n;assert(n%3==0&&n<=49152&&opacity>=0&&opacity<=1);for(unsigned i=0;i<n;++i)assert(std::isfinite(v[i].x)&&std::isfinite(v[i].y));if(preview)for(unsigned i=0;i<n;i+=3)printf("%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%.5f\n",v[i].x,v[i].y,v[i+1].x,v[i+1].y,v[i+2].x,v[i+2].y,c.r,c.g,c.b,opacity);}};
#include "gpu_cube_orbit.h"
static GpuCubeOrbit renderer;static CubeOrbitModel model;
int main(int argc,char**argv){CubeOrbitSettings settings;AudioSpectrum audio;audio.available=true;audio.peak=.5f;for(unsigned i=0;i<128;++i)audio.bands[i]=.25f+.35f*(.5f+.5f*sinf(i*.17f));
 for(int i=0;i<180;++i)model.update(audio,settings,1,1.f/60,false);assert(model.emitted>50&&model.emitted<=512);assert(model.level[0]>.1f);
 auto saved=model;model.update(audio,settings,1,.05f,true);assert(!memcmp(&saved,&model,sizeof(model)));
 auto &particle=model.particles[0];assert(particle.life>0);float x=particle.x,y=particle.y,vx=particle.vx,vy=particle.vy;AudioSpectrum quiet;model.update(quiet,settings,1,1.f/60,false);assert(fabsf(particle.x-(x+vx/60))<.001f&&fabsf(particle.y-(y+vy/60))<.001f);unsigned emitted=model.emitted;
 for(int i=0;i<300;++i)model.update(quiet,settings,1,1.f/60,false);assert(model.emitted==emitted&&model.level[0]<.001f);for(auto &p:model.particles)assert(p.life==0);
 for(int hz:{30,60,120}){CubeOrbitModel m;for(int i=0;i<hz*2;++i)m.update(audio,settings,1,1.f/hz,false);assert(fabsf(m.angle-settings.rotation*.01745329252f*2)<.001f);assert(fabsf(m.level[20]-saved.level[20])<.002f);}
 for(int row=0;row<8;++row){for(int i=0;i<1000;++i)settings.adjust(row,1);}assert(settings.radius==280&&settings.height==240&&settings.size==18&&settings.life==40);for(int row=0;row<8;++row)for(int i=0;i<1000;++i)settings.adjust(row,-1);assert(settings.radius==120&&settings.height==60&&settings.size==8&&settings.life==6);
 settings=CubeOrbitSettings{};GpuScene scene;for(int i=0;i<300;++i)renderer.draw(&scene,audio,settings,1,0,false,1.f/60);assert(calls==1500);if(argc>1){preview=true;renderer.draw(&scene,audio,settings,1,0,false,1.f/60);}else puts("PASS spectrum-driven radial cubes, bounded emission, independent tip velocity, pause, silence decay, 30/60/120Hz timing, setting bounds and five GPU batches");
 // Stress maximum geometry, invalid sample input and pool recycling.
 preview=false;settings.size=8;settings.height=240;settings.radius=280;settings.density=100;settings.life=40;for(auto &v:audio.bands)v=1;for(int i=0;i<1000;++i)renderer.draw(&scene,audio,settings,2,3,false,.05f);audio.bands[0]=NAN;renderer.draw(&scene,audio,settings,1,0,false,.016f);
}
