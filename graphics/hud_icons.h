#pragma once
#include "modern_ui.h"
struct HudIconMask {
 unsigned char coverage[32][40]{};
 struct Run {unsigned char x,y,width,coverage;};Run runs[1280]{};unsigned runCount=0;
 static bool line(float x,float y,float a,float b,float c,float d,float width=2.2f){float dx=c-a,dy=d-b,t=((x-a)*dx+(y-b)*dy)/(dx*dx+dy*dy+.00001f);if(t<0)t=0;if(t>1)t=1;dx=x-a-t*dx;dy=y-b-t*dy;return dx*dx+dy*dy<=width*width*.25f;}
 static bool triangle(float x,float y,float a,float b,float c,float d,float e,float f){float p=(x-a)*(d-b)-(y-b)*(c-a),q=(x-c)*(f-d)-(y-d)*(e-c),r=(x-e)*(b-f)-(y-f)*(a-e);return (p>=0&&q>=0&&r>=0)||(p<=0&&q<=0&&r<=0);}
 static bool ink(int k,float x,float y){
  if(k<3){bool bar=k==0?(x>=3&&x<=6&&y>=3&&y<=27):k==2?(x>=32&&x<=35&&y>=3&&y<=27):false;return bar||(k==0?triangle(x,y,9,15,29,3,29,27):triangle(x,y,10,3,30,15,10,27));}
  if(k==3){ // Break the underpass so the two shuffle routes stay distinct.
   return line(x,y,3,5,10,5)||line(x,y,10,5,29,25)||line(x,y,29,25,36,25)||line(x,y,3,25,10,25)||line(x,y,10,25,16,19)||line(x,y,24,11,29,5)||line(x,y,29,5,36,5)||line(x,y,31,1,36,5)||line(x,y,36,5,31,9)||line(x,y,31,21,36,25)||line(x,y,36,25,31,29);
  }
  if(k==4)return line(x,y,4,12,4,7)||line(x,y,4,7,34,7)||line(x,y,34,7,29,2)||line(x,y,34,7,29,12)||line(x,y,34,18,34,23)||line(x,y,34,23,4,23)||line(x,y,4,23,9,18)||line(x,y,4,23,9,28);
  if(k==5){float dx=x-29,dy=y-22,r=dx*dx+dy*dy;return line(x,y,3,3,31,3)||line(x,y,3,3,3,23)||line(x,y,3,23,17,23)||line(x,y,31,3,31,11)||(r>=49&&r<=81)||line(x,y,29,22,29,17)||line(x,y,29,22,33,24);}
  if(k==6||k==7){bool speaker=(x>=2&&x<=9&&y>=10&&y<=21)||triangle(x,y,8,10,19,3,19,28)||triangle(x,y,8,10,19,28,8,21);if(k==7)return speaker||line(x,y,25,10,35,21)||line(x,y,25,21,35,10);float dx=x-21,dy=y-15.5f,r=dx*dx+dy*dy;return speaker||(x>25&&r>=125&&r<=175);}
  // Settings sliders, alongside the distinct controller OPTIONS button.
  return line(x,y,2,7,37,7)||line(x,y,2,16,37,16)||line(x,y,2,25,37,25)||(x>=10&&x<=14&&y>=3&&y<=11)||(x>=26&&x<=30&&y>=12&&y<=20)||(x>=16&&x<=20&&y>=21&&y<=29);
 }
 HudIconMask(int kind){for(int y=0;y<32;++y)for(int x=0;x<40;++x){int n=0;for(int yy=0;yy<3;++yy)for(int xx=0;xx<3;++xx)if(ink(kind,x+(xx+.5f)/3,y+(yy+.5f)/3))++n;coverage[y][x]=(unsigned char)n;}
  // Cache disjoint spans by coverage once. Sorting these never changes pixels,
  // and avoids a GPU batch flush at almost every antialiased edge pixel.
  for(int level=1;level<=9;++level)for(int y=0;y<32;++y)for(int x=0;x<40;){int n=coverage[y][x],start=x++;while(x<40&&coverage[y][x]==n)++x;if(n==level)runs[runCount++]={(unsigned char)start,(unsigned char)y,(unsigned char)(x-start),(unsigned char)n};}
 }
};
inline const HudIconMask &hudIconMask(int kind){
 static const HudIconMask masks[]={HudIconMask(0),HudIconMask(1),HudIconMask(2),HudIconMask(3),HudIconMask(4),HudIconMask(5),HudIconMask(6),HudIconMask(7),HudIconMask(8)};
 return masks[UiSettings::bound(kind,0,8)];
}
template<class Scene> void hudHighlight(Scene *s,int kind,int x,int y){
 const auto &mask=hudIconMask(kind);int left=40,right=0,top=32,bottom=0;const int step=currentUiSettings().style==0?2:1;
 for(int yy=0;yy<32;yy+=step)for(int xx=0;xx<40;xx+=step)if(mask.coverage[yy][xx]>=(step==2?4:1)){if(xx<left)left=xx;if(yy<top)top=yy;if(xx+step>right)right=xx+step;if(yy+step>bottom)bottom=yy+step;}
 s->DrawRectangle(x+(left+right-64)/2,y+(top+bottom-64)/2,64,64,Color{0,55,72});
}
template<class Scene> void hudIcon(Scene *s,int kind,int x,int y,Color color){
 const auto &m=hudIconMask(kind);
 if(currentUiSettings().style==0){for(int yy=0;yy<32;yy+=2)for(int xx=0;xx<40;xx+=2)if(m.coverage[yy][xx]>=4)s->DrawRectangle(x+xx,y+yy,2,2,color);return;}
 const Color fg=uiColor(color);
 for(unsigned i=0;i<m.runCount;++i){const auto &r=m.runs[i];int n=r.coverage;Color bg=uiTextBackground(s,x+r.x,y+r.y,0);Color mixed{(unsigned char)((fg.r*n+bg.r*(9-n))/9),(unsigned char)((fg.g*n+bg.g*(9-n))/9),(unsigned char)((fg.b*n+bg.b*(9-n))/9)};s->DrawRectangle(x+r.x,y+r.y,r.width,1,mixed);}
}
