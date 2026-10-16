// minimal headless test harness: CHECK accumulates failures; main returns count
#pragma once
#include <cstdio>

namespace smgktest {
inline int& failures() {
    static int f = 0;
    return f;
}
} // namespace smgktest

#define CHECK(cond)                                                                                                                        \
    do {                                                                                                                                   \
        if(!(cond)) {                                                                                                                      \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                                    \
            ++smgktest::failures();                                                                                                        \
        }                                                                                                                                  \
    } while(0)

#define TEST_RETURN() return smgktest::failures() ? 1 : 0
