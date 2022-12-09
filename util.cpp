#include "util.h"


#ifdef _WIN32
#include <Windows.h>
#else
#include<stdio.h>
#include<sys/time.h>
#endif

uint64_t getSystemTime()
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval t;
    gettimeofday(&t, NULL);
    return (uint64_t)t.tv_sec * 1000 + t.tv_usec / 1000;
#endif
}
