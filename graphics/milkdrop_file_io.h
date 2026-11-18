#pragma once
#include <vector>
#include <string>
#include <stddef.h>
bool milkdropFilesInit(std::string &error);
bool readMilkdropFile(const char *path,std::vector<char> &bytes,size_t maxBytes,std::string &error);
