#pragma once
#include "preset_safety.hpp"
#include "preset_folder.hpp"
#include "import_profile.hpp"
#include "custom_shapes.hpp"
#include "custom_waves.hpp"
static int prepareImportedShaders(const char *path,std::string &error);
void prepareUsbAccess();
const char *usbAccessStatus();
// Fixed, audited bank: file parser/evaluator and CPU geometry are prepared once.
// Browsing changes only selection; X applies at a fenced frame boundary.
struct CommunityBank {
    #ifdef HYPNOTIC_TEST_FAILURE_MENU
    static constexpr unsigned builtinCount=14,capacity=192;
#else
    static constexpr unsigned builtinCount=8,capacity=192;
#endif
    unsigned count=builtinCount;
    static constexpr const char *builtinNames[14]={"SHAKE","BASS ZOOM","MICROCOSM","VOLUME ZOOM","CONSTANT VELOCITY","STARFISH","CAULDRON PAINTERLY","BRAIN ZOOM TEXTURED","TEST - UNSUPPORTED WAVE","TEST - CUSTOM SHAPE","TEST - BAD EQUATION","TEST - MISSING FILE","TEST - UNKNOWN SHADER","TEST - INVALID FRAME"};
    static constexpr const char *builtinPaths[14]={"/app0/assets/misc/community-shake.milk","/app0/assets/misc/community-bass-zoom.milk","/app0/assets/misc/community-microcosm.milk","/app0/assets/misc/community-volume-zoom.milk","/app0/assets/misc/community-constant-velocity.milk","/app0/assets/misc/community-starfish.milk","/app0/assets/misc/community-cauldron.milk","/app0/assets/misc/community-brain-zoom.milk","/app0/assets/misc/reject-wave.milk","/app0/assets/misc/reject-shape.milk","/app0/assets/misc/reject-equation.milk","/app0/assets/misc/reject-missing.milk","/app0/assets/misc/reject-shader.milk","/app0/assets/misc/reject-frame.milk"};
    PresetEquations *equations[capacity]={};LegacyShake *waves[capacity]={};SpatialWarp *warps[capacity]={};
    std::string names[capacity],paths[capacity];
    CustomWaves *extras[capacity]={};CustomShapes *shapes[capacity]={};int shaderSlots[capacity];
    bool ready[capacity]={},examined[capacity]={};std::string reasons[capacity];
    char folderStatus[100]={};
    CommunityBank(){for(auto &slot:shaderSlots)slot=-1;for(unsigned i=0;i<builtinCount;++i){names[i]=builtinNames[i];paths[i]=builtinPaths[i];}}
    ~CommunityBank(){for(unsigned i=0;i<capacity;++i){delete equations[i];delete waves[i];delete warps[i];delete shapes[i];delete extras[i];}}
    void discard(unsigned i){shaderSlots[i]=-1;delete equations[i];delete waves[i];delete warps[i];delete shapes[i];delete extras[i];extras[i]=nullptr;shapes[i]=nullptr;equations[i]=nullptr;waves[i]=nullptr;warps[i]=nullptr;}
    void scanFolders(bool external=true){
        if(external)prepareUsbAccess();
        PresetFolder folder;folder.scan("/app0/assets/curated");
        if(external){
            folder.scan("/data/hypnotic/presets");
            for(unsigned i=0;i<8;++i)folder.scan("/mnt/usb"+std::to_string(i)+"/HYPNOTIC/presets");
            printf("MILKDROP IMPORT ACCESS %s\n",usbAccessStatus());
        }
        FILE *probe=fopen("/app0/assets/audio/audio-smoke.wav","rb");
        printf("MILKDROP APP0 AFTER SCAN %s\n",probe?"PASS":"FAIL");if(probe)fclose(probe);
        mergeFolder(folder);
        snprintf(folderStatus,100,"%u IMPORTS - TRIANGLE RESCAN%s",count-builtinCount,(folder.limited || count==capacity)?" - SCAN LIMIT REACHED":"");
        printf("MILKDROP IMPORT SCAN %s OPENED %u ERROR %d\n",folderStatus,folder.opened,folder.lastError);
    }
    void mergeFolder(const PresetFolder &folder){
        const auto selectedPath=paths[selected];
        const bool imported=active>=builtinCount;
        const auto activePath=paths[active],activeName=names[active],activeReason=reasons[active];
        auto extra=extras[active];auto eq=equations[active];auto wave=waves[active];auto warp=warps[active];auto shape=shapes[active];
        const auto slot=shaderSlots[active];const bool wasReady=ready[active],wasExamined=examined[active];
        for(unsigned i=builtinCount;i<count;++i){
            if(i!=active)discard(i);
            else {equations[i]=nullptr;waves[i]=nullptr;warps[i]=nullptr;extras[i]=nullptr;shapes[i]=nullptr;shaderSlots[i]=-1;}
        }
        for(unsigned i=builtinCount;i<capacity;++i){ready[i]=examined[i]=false;names[i].clear();paths[i].clear();reasons[i].clear();}
        count=builtinCount;unsigned retained=capacity;
        bool present=false;for(unsigned i=0;i<folder.count;++i)if(folder.files[i]==activePath)present=true;
        const unsigned limit=capacity-((imported && !present)?1u:0u);
        for(unsigned i=0;i<folder.count && count<limit;++i){
            paths[count]=folder.files[i];const auto slash=paths[count].find_last_of('/');names[count]=paths[count].substr(slash+1);
            if(imported && paths[count]==activePath)retained=count;
            ++count;
        }
        if(imported){
            // Keep the running allocation, but put it at its sorted file position.
            // A removed active file remains as the final entry until switched away.
            if(retained==capacity){retained=count++;paths[retained]=activePath;names[retained]=activeName;}
            active=retained;equations[active]=eq;waves[active]=wave;warps[active]=warp;shapes[active]=shape;shaderSlots[active]=slot;
            extras[active]=extra;ready[active]=wasReady;examined[active]=wasExamined;reasons[active]=activeReason;
        }
        selected=active;for(unsigned i=0;i<count;++i)if(paths[i]==selectedPath)selected=i;
    }
    bool prepare(unsigned i){
        if(equations[i] && ready[i])return true;
        if(examined[i] && !ready[i])return false;
        examined[i]=true;
        if(i>=builtinCount && !checkImportProfile(paths[i].c_str(),reasons[i])){ready[i]=false;return false;}
        bool shaders=i==6 || i==7;
        if(i>=builtinCount){libprojectM::MilkdropPreset::PresetFileParser file;if(file.Read(paths[i]))shaders=!file.GetCode("warp_").empty() || !file.GetCode("comp_").empty();}
        equations[i]=new PresetEquations();waves[i]=new LegacyShake();warps[i]=new SpatialWarp();
        ready[i]=preparePreset(paths[i].c_str(),shaders,*equations[i],*waves[i],*warps[i],reasons[i],i>=builtinCount);
        if(ready[i]){shapes[i]=new CustomShapes();ready[i]=shapes[i]->load(paths[i].c_str(),equations[i]->initialOutput(),equations[i]->sharedMemory);if(!ready[i])reasons[i]="CUSTOM SHAPE: "+shapes[i]->error;}
        if(ready[i]){extras[i]=new CustomWaves();ready[i]=extras[i]->load(paths[i].c_str(),equations[i]->initialOutput(),equations[i]->sharedMemory);if(!ready[i])reasons[i]="CUSTOM POINTS: "+extras[i]->error;}
        if(ready[i] && shaders && i>=builtinCount){shaderSlots[i]=prepareImportedShaders(paths[i].c_str(),reasons[i]);ready[i]=shaderSlots[i]>=0;}
        if(!ready[i])discard(i);
        return ready[i];
    }
    bool errorVisible=false,stopped=false;char errorRows[3][100]={};
    unsigned lastDisplay=0;bool lastCustom=false;
    unsigned selected=1,active=1;int pad=-1;uint32_t previous=0;bool hideText=false;
    void reject(unsigned index,const char *stage,const std::string &reason){
        errorVisible=true;hideText=false;
        snprintf(errorRows[0],100,"PRESET REJECTED - %s",names[index].c_str());
        snprintf(errorRows[1],100,"%.98s",(std::string(stage)+": "+reason).c_str());
        snprintf(errorRows[2],100,"%s - UP DOWN BROWSE - X APPLY - O DISMISS",stopped?"VISUAL PAUSED":"CURRENT PRESET KEPT");
        printf("MILKDROP COMPATIBILITY REJECT %u %s STAGE %s REASON %s\n",index,names[index].c_str(),stage,reason.c_str());
    }
    void showError(){
        if(!errorVisible)return;
        for(unsigned i=0;i<3;++i)snprintf(rows[9+i],100,"%s",errorRows[i]);
    }
    bool accept(){
        if(!prepare(selected)){reject(selected,"LOAD",reasons[selected]);return false;}
        // Keep only the current imported context; apply-time allocations are bounded.
        for(unsigned i=builtinCount;i<count;++i)if(i!=selected)discard(i);
        active=selected;stopped=false;errorVisible=false;return true;
    }
    void frameError(const char *stage,const std::string &reason){
        ready[active]=false;reasons[active]=std::string(stage)+": "+reason;
        stopped=true;reject(active,stage,reason);
    }
    void load(){
        for(unsigned i=0;i<count;++i){
            prepare(i);
            printf("MILKDROP BANK %u %s %s %s\n",i,names[i].c_str(),ready[i]?"READY":"UNSUPPORTED",reasons[i].c_str());
        }
        // A broken optional preset cannot prevent startup of the rest of the bank.
        if(!ready[active])for(unsigned i=0;i<count;++i)if(ready[i]){selected=active=i;break;}
        if(!ready[active]){note("NO COMPATIBLE PRESETS - CHECK PACKAGED FILES");hold(false);}
    }
    void openPad(){
        int rc=scePadInit();if(rc!=0){printf("MILKDROP PAD INIT %08X\n",(unsigned)rc);return;}
        OrbisUserServiceInitializeParams parameters={};parameters.priority=ORBIS_KERNEL_PRIO_FIFO_LOWEST;
        sceUserServiceInitialize(&parameters);
        int32_t user=ORBIS_USER_SERVICE_USER_ID_INVALID;
        if(sceUserServiceGetInitialUser(&user)<0)return;
        pad=scePadOpen(user,ORBIS_PAD_PORT_TYPE_STANDARD,0,nullptr);
        printf("MILKDROP PAD OPEN %08X USER %08X\n",(unsigned)pad,(unsigned)user);
        if(pad<0)snprintf(rows[7],100,"NO PAD - CONNECT CONTROLLER TO PS4 AND RELAUNCH");
    }
    uint32_t buttons(){
        OrbisPadData data={};
        if(pad<0 || scePadReadState(pad,&data)<0 || !data.connected){previous=0;return 0;}
        const uint32_t pressed=data.buttons&~previous;previous=data.buttons;return pressed;
    }
};
constexpr const char *CommunityBank::builtinNames[14];
constexpr const char *CommunityBank::builtinPaths[14];
