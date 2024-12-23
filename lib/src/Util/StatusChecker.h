#pragma once
#include <ipp.h>
#include <iostream>
#include <string>
#include "JuceHeader.h"

using std::cout;
using std::endl;
using std::string;

class SingletonRepo;

class StatusChecker {
    StatusChecker() {
    }
public:

    static void report(SingletonRepo* repo, IppStatus status, int line, const char* function, const char* file);
    static void breakOnError(IppStatus status, int line, const char* function, const char* file);
};