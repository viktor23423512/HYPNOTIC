#pragma once
#ifndef HYPNOTIC_GPU_MESH_TEST
#include "gpu_scene.h"
#endif
#include "cube_orbit.h"
class GpuCubeOrbit {
 CubeOrbitModel model;
 static constexpr unsigned maxCubes=64*32+CubeOrbitModel::capacity,maxMesh=maxCubes*6;
 GpuScene::Vertex front[maxMesh],side[maxMesh],top[maxMesh],glow[maxMesh],ring[192*6];unsigned used=0;
 struct Point{float x,y;};
 static void quad(GpuScene::Vertex *out,unsigned at,Point a,Point b,Point c,Point d){out[at]={a.x,a.y,0,0};out[at+1]={b.x,b.y,0,0};out[at+2]={c.x,c.y,0,0};out[at+3]={a.x,a.y,0,0};out[at+4]={c.x,c.y,0,0};out[at+5]={d.x,d.y,0,0};}
 void cube(float x,float y,float size,float angle,float fade){
  if(used+6>maxMesh||x< -50||x>1970||y< -50||y>1130)return;
  // Fade shrinks the whole fragment to a point, keeping its three face shades.
  size*=sqrtf(CubeOrbitModel::clamp(fade,0,1));float ux=cosf(angle)*size*.5f,uy=sinf(angle)*size*.5f,tx=-uy,ty=ux;
  Point a{x-ux-tx,y-uy-ty},b{x+ux-tx,y+uy-ty},c{x+ux+tx,y+uy+ty},d{x-ux+tx,y-uy+ty};
  float dx=(ux+tx)*.42f,dy=(uy+ty)*.42f;Point aa{a.x+dx,a.y+dy},bb{b.x+dx,b.y+dy},cc{c.x+dx,c.y+dy};
  quad(front,used,a,b,c,d);quad(side,used,b,bb,cc,c);quad(top,used,a,aa,bb,b);
  float h=size*1.1f;quad(glow,used,{x-h,y-h},{x+h,y-h},{x+h,y+h},{x-h,y+h});used+=6;
 }
public:
 void draw(GpuScene *scene,const AudioSpectrum &audio,const CubeOrbitSettings &s,float gain,unsigned palette,bool paused,float dt){
  model.update(audio,s,gain,dt,paused);used=0;
  for(int col=0;col<CubeOrbitModel::columns;++col){float angle=model.angle+col*(6.28318530718f/CubeOrbitModel::columns),ux=cosf(angle),uy=sinf(angle);for(int j=0;j<model.blocks(col,s);++j){float radius=s.radius+(j+.5f)*(s.size+3);cube(960+ux*radius,540+uy*radius,(float)s.size,angle,1);}}
  for(const auto &p:model.particles)if(p.life>0)cube(p.x,p.y,p.size,p.angle,1-p.age/p.life);
  const Color colors[4]={{0,210,245},{235,30,180},{75,230,80},{245,145,25}};Color color=colors[palette%4];
  for(unsigned i=0;i<192;++i){float a=i*6.28318530718f/192,b=(i+1)*6.28318530718f/192;quad(ring,i*6,{960+cosf(a)*(s.radius-7),540+sinf(a)*(s.radius-7)},{960+cosf(b)*(s.radius-7),540+sinf(b)*(s.radius-7)},{960+cosf(b)*(s.radius-4),540+sinf(b)*(s.radius-4)},{960+cosf(a)*(s.radius-4),540+sinf(a)*(s.radius-4)});}
  scene->drawSolidMesh(ring,192*6,color,.7f);scene->drawSolidMesh(glow,used,color,.035f);scene->drawSolidMesh(side,used,color,.22f);scene->drawSolidMesh(front,used,color,.72f);scene->drawSolidMesh(top,used,Color{170,235,245},.55f);
 }
};
