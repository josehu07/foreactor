#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <filesystem>


static constexpr char DBDIR[] = "/tmp/hintdemo_dbdir";
static constexpr int NUM_LEVELS = 3;
static constexpr int FILES_PER_LEVEL = 8;
// static constexpr size_t FILE_SIZE = 16 * 1024 * 1024;
static constexpr size_t FILE_SIZE = 4096;

static const std::string table_name(int level, int index)
{
    std::stringstream ss;
    ss << level << "-" << index << ".ldb";
    return ss.str();
}


int main(void)
{
    std::filesystem::remove_all(DBDIR);
    std::filesystem::create_directory(DBDIR);
    std::filesystem::current_path(DBDIR);

    std::string dummy_content(FILE_SIZE, 'a');

    for (int level = 0; level < NUM_LEVELS; ++level) {
        for (int index = 0; index < FILES_PER_LEVEL; ++index) {
            std::ofstream table(table_name(level, index));
            table << dummy_content;
        }
    }
}
