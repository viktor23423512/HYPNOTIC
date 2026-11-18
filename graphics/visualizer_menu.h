#pragma once
#include "status_text.h"
#include "visualizer_library.h"
template<class Scene> void drawVisualizerLibrary(Scene *scene,const VisualizerLibrary &v,int active,const char *error){
 scene->DrawRectangle(48,164,1824,636,Color{4,10,16});drawStatusText(scene,"HYPNOTIC / VISUALIZERS",194,72,45);
 char text[120];snprintf(text,sizeof(text),"%u / %u",v.count?v.selection+1:0,v.count);drawStatusText(scene,text,194,1620,12);
 int first=v.tab-2;if(first<0)first=0;if(first>VisualizerLibrary::tabCount-5)first=VisualizerLibrary::tabCount-5;
 drawStatusText(scene,"\x82",254,72,1);drawStatusText(scene,"\x83",254,1812,1);
 for(int t=first;t<first+5;++t){int x=120+(t-first)*330;bool selected=t==v.tab&&!v.searching;scene->DrawRectangle(x,240,324,48,selected?Color{0,200,230}:Color{18,35,44});drawStatusText(scene,"\x8c",254,x+12,1,selected?Color{0,20,26}:Color{160,180,190});drawStatusText(scene,v.tabName(t),254,x+42,15,selected?Color{0,20,26}:Color{160,180,190});}
 scene->DrawRectangle(64,288,1792,3,Color{0,200,230});
 const int start=v.selection/6*6;
 if(v.searching){snprintf(text,sizeof(text),"SEARCH: %.72s",v.query.c_str());drawStatusText(scene,text,310,80,88);}
 for(int row=start;row<v.count&&row<start+6;++row){int y=356+(row-start)*54;bool selected=row==v.selection;if(selected){scene->DrawRectangle(64,y-10,1792,44,Color{0,55,72});scene->DrawRectangle(64,y-10,4,44,Color{0,200,230});}
  snprintf(text,sizeof(text),"%02u",row+1);drawStatusText(scene,text,y,80,4);drawStatusText(scene,v.name(v.rows[row]),y,164,79);
  if(v.rows[row]==active)drawStatusText(scene,"\x88",y,1728,1,Color{0,200,230});if(v.favorite(v.rows[row]))drawStatusText(scene,"\x8b",y,1792,1,Color{0,200,230});
 }
 if(!v.count)drawStatusText(scene,v.searching?"NO MATCHING PRESETS":v.tab==1?"NO FAVORITES - PRESS R3 ON A PRESET TO ADD ONE":v.tab==2?"APPLIED VISUALIZERS WILL APPEAR HERE":"NO IMPORTS - USB / HYPNOTIC / PRESETS",402,80,94);
 if(error&&*error){snprintf(text,sizeof(text),"PRESET UNAVAILABLE - %.77s",error);drawStatusText(scene,text,700,80,96);}
 else if(v.searching)drawStatusText(scene,"\x86 BACK TO FOLDER - \x8e EDIT SEARCH",700,80,88);
 else{const int id=v.selected();if(id==-5)drawStatusText(scene,"\x85 MATRIX SETTINGS",700,80,35);else if(id==-6||id==-7||id==-10)drawStatusText(scene,"\x85 SETTINGS",700,80,35);else if(id==-9)drawStatusText(scene,"\x85 RAIN ON / OFF",700,80,35);else if(v.tab==13)drawStatusText(scene,"\x85 RESCAN IMPORTS",700,80,35);}
 scene->DrawRectangle(64,744,1792,2,Color{27,45,55});drawStatusText(scene,"\x82\x83 FOLDERS  \x80\x81 PRESETS  X APPLY  R3 FAVORITE  \x8e SEARCH  \x86 CLOSE",768,80,98);
}
