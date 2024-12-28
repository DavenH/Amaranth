#ifndef WINHEADER_H_
#define WINHEADER_H_

    #define WINVER 0x0501
    #define _WIN32_WINNT 0x0501
  #ifdef _MSC_VER
    #define NOGDI
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX

    #include <Windows.h>

    #undef NOMINMAX
    #undef WIN32_LEAN_AND_MEAN
    #undef NOGDI
  #endif
#endif
