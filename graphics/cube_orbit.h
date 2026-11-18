#pragma once
#include "audio_spectrum.h"
#include <math.h>
struct CubeOrbitSettings {
 int rotation=18,radius=190,height=170,size=11,density=55,speed=170,life=20;bool clockwise=true;
 static int bound(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
 void adjust(int row,int d){switch(row){case 0:rotation=bound(rotation+d*3,0,60);break;case 1:clockwise=!clockwise;break;case 2:radius=bound(radius+d*10,120,280);break;case 3:height=bound(height+d*10,60,240);break;case 4:size=bound(size+d,8,18);break;case 5:density=bound(density+d*5,0,100);break;case 6:speed=bound(speed+d*20,60,400);break;case 7:life=bound(life+d*2,6,40);break;}}
};
// Fixed-size emitters and particle pool. A detached cube has its own world-space
// velocity, including the ring's tangential velocity at the moment it leaves.
struct CubeOrbitModel {
 static constexpr int columns=64,capacity=512;float level[columns]={},emit[columns]={},angle=0;
 struct Particle{float x=0,y=0,vx=0,vy=0,angle=0,spin=0,age=0,life=0,size=0;};Particle particles[capacity];
 unsigned cursor=0,emitted=0;uint32_t rng=0x43554245;
 float random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (rng>>8)*(1.f/16777216);}
 static float clamp(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
 int blocks(int col,const CubeOrbitSettings &s)const{return 1+(int)(level[col]*s.height/(s.size+3));}
 void update(const AudioSpectrum &audio,const CubeOrbitSettings &s,float gain,float dt,bool paused){
  if(paused||!isfinite(dt)||dt<=0)return;dt=clamp(dt,0,.05f);
  const float omega=s.rotation*.01745329252f*(s.clockwise?1:-1);angle=fmodf(angle+omega*dt,6.28318530718f);
  for(auto &p:particles)if(p.life>0){p.age+=dt;if(p.age>=p.life){p.life=0;continue;}p.x+=p.vx*dt;p.y+=p.vy*dt;p.angle+=p.spin*dt;}
  for(int col=0;col<columns;++col){
   // Mirror bass, mids and treble around the rotating ring.
   float f=fabsf(2.f*col/(columns-1)-1.f);float at=f*(AUDIO_SPECTRUM_BANDS-1);int band=(int)at;float frac=at-band;
   float a=audio.available?audio.bands[band]:0,b=audio.available?audio.bands[band<127?band+1:band]:0;
   float target=(isfinite(a)&&isfinite(b))?clamp((a+(b-a)*frac)*gain,0,1):0;
   const float old=level[col];level[col]+=(target-old)*(1-expf(-dt*(target>old?24.f:5.f)));
   if(!audio.available||target<.025f||s.density==0){emit[col]=0;continue;}
   float onset=target>old+.06f?2.5f:0;emit[col]+=dt*(.25f+level[col]*2.5f+onset)*s.density*.01f;
   if(emit[col]<1)continue;emit[col]-=1;
   unsigned slot=capacity;for(unsigned i=0;i<capacity;++i){unsigned j=(cursor+i)%capacity;if(particles[j].life<=0){slot=j;break;}}if(slot==capacity)continue;
   cursor=(slot+1)%capacity;float theta=angle+col*(6.28318530718f/columns),ux=cosf(theta),uy=sinf(theta);
   float distance=s.radius+(blocks(col,s)-.5f)*(s.size+3),speed=s.speed*(.65f+random()*.7f),life=s.life*.1f*(.8f+random()*.4f);
   particles[slot]={960+ux*distance,540+uy*distance,ux*speed-uy*omega*distance,uy*speed+ux*omega*distance,theta,(random()-.5f)*3,0,life,(float)s.size};++emitted;
  }
 }
};
