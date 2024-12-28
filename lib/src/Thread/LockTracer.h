#pragma once
#include "../App/SingletonAccessor.h"

class LockTracer : public SingletonAccessor {
public:
    LockTracer(SingletonRepo* repo, void* lockAddr) :
            SingletonAccessor(repo, "LockTracker") {
#ifdef TRACE_LOCKS
        threadId    = ((int)Thread::getCurrentThreadId());
        lockId      = ((int)lockAddr) % 10000;
        sout << "    " << threadId << "\t" << lockId << "\n";
#endif
    }

    LockTracer(SingletonRepo* repo, const String& lockStr) :
            SingletonAccessor(repo, "LockTracker"), lockStr(lockStr), lockId(-1) {
#ifdef TRACE_LOCKS
        threadId    = ((int)Thread::getCurrentThreadId());
//      sout << "    " << threadId << "\t" << lockStr << "\n";
#endif
    }

    ~LockTracer() override {
#ifdef TRACE_LOCKS
        if(lockId >= 0)
            sout << "### " << threadId << "\t" << lockId << "\n";
        else
            sout << "### " << threadId << "\t" << lockStr << "\n";
#endif
    }

private:
    int lockId, threadId;
    String lockStr;
};
