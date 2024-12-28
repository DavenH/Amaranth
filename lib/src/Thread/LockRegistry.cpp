#include "LockRegistry.h"

LockRegistry::LockRegistry(SingletonRepo* repo) : SingletonAccessor(repo, "LockRegistry") {
}

// want to detect T1 takes L1, T2 takes L2, T1 tries to take L2, T2 tries to take L1

// T        L
// 0x1      0x123
// 0x2      0x124
// 0x1      0x124
// 0x2      0x123  <--- deny lock.

void LockRegistry::lockEntered(void* address) {
    // LockEvent event;
    // event.lockId = reinterpret_cast<int>(address);
    // event.threadId = reinterpret_cast<int>(Thread::getCurrentThreadId());
    // event.isEnter = true;

    // todo
}

void LockRegistry::lockExited(void* address) {
 //    LockEvent event;
 //
    // event.lockId     = reinterpret_cast<int>(address);
    // event.threadId   = reinterpret_cast<int>(Thread::getCurrentThreadId());
    // event.isEnter    = false;

    // todo
}


