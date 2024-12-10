#pragma once
#include "../App/SingletonAccessor.h"
#include "../App/SingletonRepo.h"

template<class T>
class ScopedFunction {
public:
    typedef void (T::*FunctionType)();

    ScopedFunction(T* instance, FunctionType doNow, FunctionType doLater) :
            instance(instance), doLater(doLater) {
        ((*instance).*doNow)();
    }

    ~ScopedFunction() {
        ((*instance).*doLater)();
    }

    JUCE_DECLARE_NON_COPYABLE(ScopedFunction);

private:
    T* instance;
    const FunctionType doLater;
};


class GlobalScopedFunction : public SingletonAccessor {
public:
    typedef void (* FunctionType)(SingletonRepo*);

    GlobalScopedFunction(FunctionType doNow, FunctionType doLater, SingletonRepo* repo) :
            SingletonAccessor(repo, "GlobalScopedFunction"), doLater(doLater) {
        doNow(repo);
    }

    ~GlobalScopedFunction() {
        doLater(repo);
    }

    JUCE_DECLARE_NON_COPYABLE(GlobalScopedFunction);
private:
    const FunctionType doLater;
};
