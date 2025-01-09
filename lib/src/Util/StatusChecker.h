#pragma once
#include <iostream>
#include <string>

#include "JuceHeader.h"

using std::cout;
using std::endl;
using std::string;
#ifdef USE_IPP
    #include <ipp.h>
#endif

class SingletonRepo;

class StatusChecker {
    StatusChecker() {
    }
public:
#ifdef USE_IPP
    static void report(SingletonRepo* repo, IppStatus status, int line, const char* function, const char* file);
    static void breakOnError(IppStatus status, int line, const char* function, const char* file);
#endif
};