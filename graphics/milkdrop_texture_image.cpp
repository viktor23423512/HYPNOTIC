#define STB_IMAGE_STATIC
#include "milkdrop_file_io.h"
#include "../tests/milkdrop_ps4/native_graphics/texture_image.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_PKM
#define STBI_NO_DDS
#define STBI_NO_PVR
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 1024
#include "../tests/milkdrop_spike/projectm-4.1.8/vendor/SOIL2/stb_image.h"
bool loadTextureImage(const char *path,TextureImage &out){
    freeTextureImage(out);out.error.clear();int channels=0;std::vector<char> bytes;
    if(!readMilkdropFile(path,bytes,16*1024*1024,out.error))return false;
    if(!path || !stbi_info_from_memory((const stbi_uc*)bytes.data(),(int)bytes.size(),&out.width,&out.height,&channels) || out.width<1 || out.height<1 || out.width>1024 || out.height>1024){out.error="Missing, invalid or oversized PNG/JPEG texture";return false;}
    out.rgba=stbi_load_from_memory((const stbi_uc*)bytes.data(),(int)bytes.size(),&out.width,&out.height,&channels,4);
    if(!out.rgba){out.error=stbi_failure_reason()?stbi_failure_reason():"Texture decoding failed";return false;}
    return true;
}
void freeTextureImage(TextureImage &image){if(image.rgba)stbi_image_free(image.rgba);image.rgba=nullptr;image.width=image.height=0;}
