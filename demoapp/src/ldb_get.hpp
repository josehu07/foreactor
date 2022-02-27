#include <vector>
#include <sstream>


#ifndef __DEMOAPP_LDBGET_H__
#define __DEMOAPP_LDBGET_H__


static constexpr char DBDIR[] = "/tmp/demo_dbdir";
static constexpr int NUM_LEVELS = 4;
static constexpr int FILES_PER_LEVEL = 8;
static constexpr size_t FILE_SIZE = 4096;


static const std::string table_name(int level, int index) {
    std::stringstream ss;
    ss << level << "-" << index << ".ldb";
    return ss.str();
}


std::vector<std::string> ldb_get(std::vector<std::vector<int>>& files);


#endif
