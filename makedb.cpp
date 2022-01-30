#include <iostream>
#include <string>
#include <filesystem>


static constexpr std::string DBDIR = "/tmp/hintdemo_dbdir";
static constexpr std::string DUMMY(4095, "a");

static const std::string table_name(int level, int index) {
    std::stringstream ss;
    ss << level << "-" << index << ".ldb";
    return ss.str();
}


int main(void)
{
    std::filesystem::remove_all(DBDIR);
    std::filesystem::create_directory(DBDIR);
    std::filesystem::current_path(DBDIR);

    for (int level = 0; level < 3; ++level) {
        for (int index = 0; index < 7; ++index) {
            std::ofstream table(table_name(level, index));
            table << DUMMY;
        }
    }
}
