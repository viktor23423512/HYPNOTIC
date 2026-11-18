#pragma once
#include "status_text.h"
#include "cube_orbit.h"
static const char *sandVariationName(int i){static const char *names[]={"ORIGINAL","ORNATE","CELLS","WEAVE"};return names[i%4];}
template<class Scene> void drawBuiltinSettings(Scene *scene,int preset,int row,int variation,int sensitivity,int palette,const CubeOrbitSettings &s,bool midSide=true,int trail=2){
 scene->DrawRectangle(48,164,1824,636,Color{4,10,16});drawStatusText(scene,preset==5?"CYMATICS SAND / SETTINGS":preset==6?"OSCILLOSCOPE / SETTINGS":"CUBE ORBIT EQ / SETTINGS",194,80,90);
 int count=preset==5?3:preset==6?4:10;
 for(int i=0;i<count;++i){int y=270+i*43;if(i==row)scene->DrawRectangle(64,y-7,1792,35,Color{0,55,72});char text[112];
  if(preset==5){if(i==0)snprintf(text,sizeof(text),"PATTERN                %s",sandVariationName(variation));if(i==1)snprintf(text,sizeof(text),"SENSITIVITY            %.2f X",.5f+sensitivity*.25f);if(i==2)snprintf(text,sizeof(text),"COLOR                  %d / 4",palette+1);}
  else if(preset==6){const char *trails[]={"0.2","0.5","0.8","1.2"};if(i==0)snprintf(text,sizeof(text),"MODE                   %s",midSide?"MID/SIDE":"XY");if(i==1)snprintf(text,sizeof(text),"TRAIL                  %s S",trails[trail%4]);if(i==2)snprintf(text,sizeof(text),"SENSITIVITY            %.2f X",.5f+sensitivity*.25f);if(i==3)snprintf(text,sizeof(text),"COLOR                  %d / 4",palette+1);}
  else switch(i){case 0:snprintf(text,sizeof(text),"ROTATION SPEED         %d DEG/S",s.rotation);break;case 1:snprintf(text,sizeof(text),"DIRECTION              %s",s.clockwise?"CLOCKWISE":"COUNTERCLOCKWISE");break;case 2:snprintf(text,sizeof(text),"CIRCLE SIZE            %d",s.radius);break;case 3:snprintf(text,sizeof(text),"EQ HEIGHT              %d",s.height);break;case 4:snprintf(text,sizeof(text),"CUBE SIZE              %d",s.size);break;case 5:snprintf(text,sizeof(text),"BREAKAWAY DENSITY      %d PERCENT",s.density);break;case 6:snprintf(text,sizeof(text),"FLYING SPEED           %d PX/S",s.speed);break;case 7:snprintf(text,sizeof(text),"FRAGMENT LIFETIME      %.1f S",s.life*.1f);break;case 8:snprintf(text,sizeof(text),"SENSITIVITY            %.2f X",.5f+sensitivity*.25f);break;default:snprintf(text,sizeof(text),"COLOR                  %d / 4",palette+1);break;}
  drawStatusText(scene,text,y,80,96);
 }
 scene->DrawRectangle(64,744,1792,2,Color{27,45,55});drawStatusText(scene,"\x80\x81 SELECT  \x82\x83 ADJUST  \x85 / \x86 BACK",768,80,96);
}
