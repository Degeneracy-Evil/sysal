#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

inline int g_test_pass = 0;
inline int g_test_fail = 0;

#define CHECK(expr)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if(!(expr))                                                                                                    \
        {                                                                                                              \
            std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr);                                               \
            ++g_test_fail;                                                                                             \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            ++g_test_pass;                                                                                             \
        }                                                                                                              \
    } while(0)

#define TEST_SUMMARY()                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        std::printf("  %d passed, %d failed\n", g_test_pass, g_test_fail);                                             \
        return g_test_fail == 0 ? 0 : 1;                                                                               \
    } while(0)
