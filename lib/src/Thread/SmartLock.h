#pragma once
#include "../App/SingletonAccessor.h"
#include "JuceHeader.h"

class SmartLock : public SingletonAccessor {
    SmartLock(SingletonRepo* repo, CriticalSection& lock);
    ~SmartLock() override;

private:
    ScopedLock sl;
};
