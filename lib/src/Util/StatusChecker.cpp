#include "StatusChecker.h"
#include "../App/SingletonRepo.h"
#include "../Definitions.h"

#ifdef USE_IPP
#include <ipp.h>
void
StatusChecker::report(SingletonRepo* repo, const IppStatus status, int line, const char* function, const char* file) {
    if (status != ippStsNoErr && status != ippStsNoOperation) {
        cout << ippGetStatusString(status) << "\n"; // << ippErrors[status] << "\n";
        cout << "\t" << line << ", " << function << ", " << file << "\n";
    }
}

void StatusChecker::breakOnError(const IppStatus status, int line, const char* function, const char* file) {
    if (status != ippStsNoErr && status != ippStsNoOperation) {
        const char* error = ippGetStatusString(status);
        jassertfalse;
    }
}

#endif