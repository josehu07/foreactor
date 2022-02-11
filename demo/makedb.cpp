#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <filesystem>


static constexpr char DBDIR[] = "/tmp/hintdemo_dbdir";
static constexpr int NUM_LEVELS = 4;
static constexpr int FILES_PER_LEVEL = 8;
static constexpr size_t FILE_SIZE = 4096;

static const std::string table_name(int level, int index) {
    std::stringstream ss;
    ss << level << "-" << index << ".ldb";
    return ss.str();
}


static const std::string rand_string(size_t length) {
    static constexpr char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::string str;
    str.reserve(length);

    for (size_t i = 0; i < length; ++i)
        str += alphanum[rand() % (sizeof(alphanum) - 1)];

    return str;
}


int main(void) {
    std::filesystem::remove_all(DBDIR);
    std::filesystem::create_directory(DBDIR);
    std::filesystem::current_path(DBDIR);

    for (int level = 0; level < NUM_LEVELS; ++level) {
        for (int index = 0; index < FILES_PER_LEVEL; ++index) {
            std::ofstream table(table_name(level, index));
            std::string rand_content = rand_string(FILE_SIZE);
            table << rand_content;
        }
    }
}
