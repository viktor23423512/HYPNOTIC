#include <cstdio>
#include <cassert>
#include <cstring>
#include <string>
#include "track_library.h"
#include "usb_folder_library.h"
static TrackLibrary tracks;static UsbFolderLibrary folders;static bool ready=false;
bool usbLibraryReady(){return ready;}bool usbLibraryLimited(){return false;}
int usbFolderCount(){return ready?folders.count:0;}int usbTrackCount(){return ready?tracks.count:0;}
const char *usbFolderPath(int i){return folders.paths[i];}int usbFolderParent(int i){return folders.parent[i];}
int usbTrackFolder(int i){return tracks.folder[i];}const char *usbTrackName(int i){return TrackLibrary::name(tracks.paths[i]);}
int usbActiveTrack(){return 1;}bool usbAudioFinished(){return false;}uint64_t usbTrackDurationMs(int){return 184000;}
struct Color{int r,g,b;};static bool preview=false;
struct Scene{void DrawRectangle(int x,int y,int w,int h,Color c){assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=1920&&y+h<=1080);if(preview)printf("%d,%d,%d,%d,%d,%d,%d\n",x,y,w,h,c.r,c.g,c.b);}};
#include "music_menu.h"
#include "player_hud.h"
static MusicFolderBrowser browser;static MusicNavigation nav;
int main(int argc,char**argv){
 int usb0=folders.add("/mnt/usb0",-1),empty=folders.add("/mnt/usb1",-1),usb2=folders.add("/mnt/usb2",-1);
 int game=folders.add("/mnt/usb0/ITEMZFLOW",usb0),music=folders.add("/mnt/usb0/Music",usb0),album=folders.add("/mnt/usb0/Music/Album",music);
 tracks.add("/mnt/usb0/root.wav",184000,usb0);tracks.add("/mnt/usb0/Music/Album/song.ogg",184000,album);tracks.add("/mnt/usb2/Other.flac",184000,usb2);
 nav.refresh(browser);assert(!nav.initialized&&browser.count==0);ready=true;nav.refresh(browser);
 assert(nav.source==1&&nav.rootCount==2&&browser.folder==usb0&&browser.count==2);assert(!nav.audible[empty]&&!nav.audible[game]);
 assert(browser.entries[0].folder);assert(nav.apply(browser)==-1&&browser.folder==music);assert(nav.apply(browser)==-1&&browser.folder==album);assert(nav.apply(browser)==1);
 nav.changeSource(1,browser);assert(nav.source==2&&browser.folder==usb2);nav.changeSource(-1,browser);assert(browser.folder==album);
 nav.search("OTHER",browser);assert(nav.searchResults&&browser.count==1&&nav.apply(browser)==2);assert(nav.back(browser)&&browser.folder==album);
 assert(nav.back(browser)&&browser.folder==music);assert(nav.back(browser)&&browser.folder==usb0);assert(!nav.back(browser));
 nav.changeSource(-1,browser);assert(nav.source==0&&browser.searching&&browser.count==3);nav.search("SONG",browser);assert(browser.count==1);assert(nav.back(browser)&&nav.source==0&&browser.count==3);assert(!nav.back(browser));
 // A large catalogue still pages/selects global track IDs, and tabs remember rows.
 for(int i=0;i<1000;++i){char path[128];snprintf(path,sizeof(path),"/mnt/usb0/Music/Album/Track%04d.mp3",i);assert(tracks.add(path,184000,album));}
 nav.refresh(browser);assert(browser.count==1003);browser.move(996);assert(browser.selection==996);const int global=browser.entries[996].index;assert(nav.apply(browser)==global);nav.changeSource(1,browser);nav.changeSource(-1,browser);assert(browser.selection==996);
 if(argc>1){preview=true;Scene scene;nav.changeSource(1,browser);nav.apply(browser);nav.apply(browser);drawMusicLibrary(&scene,nav,browser,true);drawPlayerHud(&scene,PlayerHudData{"SONG.OGG","MATRIX EQ",64000,184000,true,0,-1,true,30,false});}else puts("PASS music tabs, audio-only folders/roots, remembered locations, search/back, original playback IDs and 1003-track navigation");
}
