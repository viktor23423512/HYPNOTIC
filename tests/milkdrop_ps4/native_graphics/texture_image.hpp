#pragma once
#include <string>
struct TextureImage { unsigned char *rgba=nullptr;int width=0,height=0;std::string error; };
bool loadTextureImage(const char *path,TextureImage &out);
void freeTextureImage(TextureImage &image);
