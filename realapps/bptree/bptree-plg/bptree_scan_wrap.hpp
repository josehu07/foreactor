// This plugin is included directly by bptree source code for convenience.


#include <vector>
#include <foreactor.h>

#pragma once


void PluginScanPrologue(int fd, std::vector<uint64_t> *leaves);
void PluginScanEpilogue();
