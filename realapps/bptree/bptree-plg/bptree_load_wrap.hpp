// This plugin is included directly by bptree source code for convenience.


#include <foreactor.h>

#include "common.hpp"

#pragma once


void PluginLoadPrologue(int fd, std::vector<uint64_t> *leaves,
                        std::vector<bptree::Page *> *leaves_pages);
void PluginLoadEpilogue();
