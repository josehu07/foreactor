#include <functional>
#include <vector>
#include <string>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>


#ifndef __DEMO_HIJACKEES_H__
#define __DEMO_HIJACKEES_H__


struct ExperArgs {};

typedef std::function<void(void *)> ExperFunc;


struct ExperSimpleArgs : ExperArgs {
    const std::string filename;
    const std::string wcontent;
    const size_t wlen;
    const size_t rlen;
    char * const rbuf0;
    char * const rbuf1;

    ExperSimpleArgs(std::string filename, std::string wcontent)
        : filename(filename), wcontent(wcontent), wlen(wcontent.length()),
          rlen(wlen / 2), rbuf0(new char[rlen]), rbuf1(new char[rlen]) {
        assert(rbuf0 != nullptr);
        assert(rbuf1 != nullptr);
    }
    ~ExperSimpleArgs() {
        delete[] rbuf0;
        delete[] rbuf1;
    }
};

void exper_simple(void *args);


#endif
