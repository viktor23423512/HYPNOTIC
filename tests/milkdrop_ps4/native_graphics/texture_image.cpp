#include "texture_image.hpp"
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
#include "../../../tests/milkdrop_spike/projectm-4.1.8/vendor/SOIL2/stb_image.h"
bool loadTextureImage(const char *path,TextureImage &out){
    freeTextureImage(out);out.error.clear();int channels=0;
    if(!path || !stbi_info(path,&out.width,&out.height,&channels) || out.width<1 || out.height<1 || out.width>1024 || out.height>1024){out.error="Missing, invalid or oversized PNG/JPEG texture";return false;}
    out.rgba=stbi_load(path,&out.width,&out.height,&channels,4);
    if(!out.rgba){out.error=stbi_failure_reason()?stbi_failure_reason():"Texture decoding failed";return false;}
    return true;
}
void freeTextureImage(TextureImage &image){if(image.rgba)stbi_image_free(image.rgba);image.rgba=nullptr;image.width=image.height=0;}
