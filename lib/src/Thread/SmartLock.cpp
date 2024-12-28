#include "SmartLock.h"

#define CHECK_LOCKS

SmartLock::SmartLock(SingletonRepo* repo, CriticalSection& lock) :
        SingletonAccessor(repo, "SmartLock"), sl(lock) {
#ifdef CHECK_LOCKS
//  getObj(LockRegistry).lockEntered(&lock);
#endif
}

SmartLock::~SmartLock() {
#ifdef CHECK_LOCKS
//  getObj(LockRegistry).lockExited(&lock);
#endif
}
