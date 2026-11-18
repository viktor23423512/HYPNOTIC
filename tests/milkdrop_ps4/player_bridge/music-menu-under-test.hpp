void drawMusicMenu(GpuScene *scene, const VisualizerState &state)
{
    scene->DrawRectangle(48, 272, 1824, 574, Color{4, 10, 16});
    if(musicSearch.open){
        drawStatusText(scene,"SEARCH USB MUSIC",292);char query[96];snprintf(query,sizeof(query),"> %s_",musicSearch.text);drawStatusText(scene,query,350);
        drawStatusText(scene,"FIND FILE NAMES ACROSS ALL USB FOLDERS",400);
        for(int i=0;i<40;++i){int x=64+i%10*178,y=458+i/10*60;if(i==musicSearch.selected)scene->DrawRectangle(x,y-5,164,42,Color{0,55,72});char key[8]={MusicSearchInput::keys()[i],0};if(i==36)snprintf(key,sizeof(key),"SPACE");if(i==38)snprintf(key,sizeof(key),"UNDER");if(i==39)snprintf(key,sizeof(key),"DOT");drawStatusText(scene,key,y,x+12,7);}
        for(int i=0;i<2;++i){int x=64+i*890;if(musicSearch.selected==40+i)scene->DrawRectangle(x,716,868,44,Color{0,55,72});drawStatusText(scene,i?"CANCEL":"SEARCH",722,x+12,12);}
        drawStatusText(scene,"X TYPE / APPLY - \x85 DELETE - \x84 CLEAR - \x86 CANCEL - OPTIONS SEARCH",796);return;
    }
    if(state.microphone)
    {
        drawStatusText(scene,"HEADSET MICROPHONE - 16000 HZ MONO",292);
        drawStatusText(scene,microphoneStatus(),358);
        drawStatusText(scene,"SPEAK OR TAP THE MIC - WATCH THE LEVEL",430);
        scene->DrawRectangle(64,490,1776,24,Color{12,25,30});
        const int fill=microphonePeak()*1776/32768;
        if(fill)scene->DrawRectangle(64,490,fill,24,Color{30,210,100});
        drawStatusText(scene,microphoneDetails(),550);
        drawStatusText(scene,"CAPTURE ONLY - NO SPEAKER PLAYBACK OR RECORDING",626);
        drawStatusText(scene,"\x85 USB MUSIC - X VISUALIZE - \x86 CLOSE",774);
        return;
    }
    musicBrowser.refresh();
    drawStatusText(scene,"MUSIC - TOUCHPAD SEARCH - \x85 MIC TEST",292);
    char searchPath[96];snprintf(searchPath,sizeof(searchPath),"SEARCH: %s",musicBrowser.query);const char *path=musicBrowser.searching?searchPath:musicBrowser.path();char location[128];const size_t pathLength=strlen(path);
    snprintf(location,sizeof(location),"%s%.94s",pathLength>94?"...":"",path+(pathLength>94?pathLength-94:0));drawStatusText(scene,location,334);
    if(!usbLibraryReady())drawStatusText(scene,"SCANNING USB FOLDERS - PLEASE WAIT",390);
    else if(!musicBrowser.count)drawStatusText(scene,musicBrowser.searching?"NO MATCHING TRACKS - TOUCHPAD TO EDIT SEARCH":musicBrowser.folder<0?"NO USB DRIVE FOUND - CONNECT AND RELAUNCH":"NO SUPPORTED MUSIC OR FOLDERS HERE",390);
    const int first=(musicBrowser.selection/7)*7;
    for(int row=0;row<7 && first+row<musicBrowser.count;++row){
        const int index=first+row;const auto &entry=musicBrowser.entries[index];const int y=374+row*44;
        if(index==musicBrowser.selection)scene->DrawRectangle(56,y-5,1800,34,Color{0,55,72});
        char label[128];snprintf(label,sizeof(label),"%02d %s %.66s",index+1,entry.folder?"FOLDER ":entry.index==usbActiveTrack()?"CURRENT":"       ",musicBrowser.name(entry));drawStatusText(scene,label,y);
        if(!entry.folder){const uint64_t seconds=usbTrackDurationMs(entry.index)/1000;char duration[32];snprintf(duration,sizeof(duration),"%02llu:%02llu",(unsigned long long)(seconds/60),(unsigned long long)(seconds%60));drawStatusText(scene,duration,y,1600,12);}
    }
    const uint64_t total = usbPlaybackDurationMs();
    const uint64_t position = usbPlaybackPositionMs();
    const uint64_t safePosition = position < total ? position : total;
    scene->DrawRectangle(64, 678, 1776, 12, Color{20, 35, 44});
    const int fill = total ? (int)(safePosition * 1776 / total) : 0;
    if (fill) scene->DrawRectangle(64, 678, fill, 12, Color{0, 200, 230});
    char progress[128];
    snprintf(progress, sizeof(progress), "CURRENT %02llu:%02llu / %02llu:%02llu - L1 R1 JUMP 10S - L2 R2 PRESSURE SCRUB",
        (unsigned long long)(position / 60000), (unsigned long long)(position / 1000 % 60),
        (unsigned long long)(total / 60000), (unsigned long long)(total / 1000 % 60));
    drawStatusText(scene, progress, 710);
    char footer[128];
    snprintf(footer,sizeof(footer),"%d ITEMS - \x80\x81 SELECT - \x82\x83 PAGE - X OPEN / PLAY - \x86 %s - \x84 VISUALIZERS",musicBrowser.count,musicBrowser.searching?"BACK":musicBrowser.folder<0?"CLOSE":"BACK");
    if(usbLibraryLimited())drawStatusText(scene,"SCAN LIMIT REACHED - SOME FILES OR DEEP FOLDERS WERE OMITTED",744);
    else if(musicBrowser.searching && musicBrowser.count){const auto &e=musicBrowser.entries[musicBrowser.selection];const char *parent=usbFolderPath(usbTrackFolder(e.index));char location[112];size_t n=strlen(parent);snprintf(location,sizeof(location),"IN: %s%.88s",n>88?"...":"",parent+(n>88?n-88:0));drawStatusText(scene,location,744);}
    drawStatusText(scene, footer, 796);
}

