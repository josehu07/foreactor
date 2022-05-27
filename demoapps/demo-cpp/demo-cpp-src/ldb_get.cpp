#include <string>
#include <vector>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "ldb_get.hpp"


static constexpr int TERMINATES_AT = 3;


std::vector<std::string> ldb_get(std::vector<std::vector<int>>& files) {
    std::vector<std::string> bytes;
    char read_buf[FILE_SIZE + 1];

    for (int index = FILES_PER_LEVEL - 1; index >= TERMINATES_AT; --index) {
        if (files[0][index] < 0)
            files[0][index] = open(table_name(0, index).c_str(), 0, O_RDONLY);
        ssize_t ret __attribute__((unused)) =
            pread(files[0][index], read_buf, FILE_SIZE, 0);
        assert(ret == FILE_SIZE);

        read_buf[FILE_SIZE] = '\0';
        bytes.push_back(std::string(read_buf));
    }

    return bytes;
}
