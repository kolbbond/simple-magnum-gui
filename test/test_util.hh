// minimal headless test harness: CHECK accumulates failures; main returns count
#pragma once
#include <cmath>
#include <cstdio>

namespace smgtest {
inline int& failures() {
    static int f = 0;
    return f;
}
inline bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }
} // namespace smgtest

#define CHECK(cond)                                                                                                                        \
    do {                                                                                                                                   \
        if(!(cond)) {                                                                                                                      \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                                    \
            ++smgtest::failures();                                                                                                         \
        }                                                                                                                                  \
    } while(0)

#define TEST_RETURN() return smgtest::failures() ? 1 : 0
