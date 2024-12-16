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

class ScopedLambda {
    public:
    template<typename F1, typename F2>
    ScopedLambda(F1&& doNow, F2&& doLater) :
        laterFn(std::forward<F2>(doLater)) {
        doNow();
    }

    ~ScopedLambda() {
        laterFn();
    }

    JUCE_DECLARE_NON_COPYABLE(ScopedLambda);

private:
    std::function<void()> laterFn;
};

class GlobalScopedFunction : public SingletonAccessor {
public:
    typedef void (* FunctionType)(SingletonRepo*);

    GlobalScopedFunction(
        FunctionType doNow,
        FunctionType doLater,
        SingletonRepo* repo) :
            SingletonAccessor(repo, "GlobalScopedFunction"), doLater(doLater) {
        doNow(repo);
    }

    ~GlobalScopedFunction() override {
        doLater(repo);
    }

    JUCE_DECLARE_NON_COPYABLE(GlobalScopedFunction);
private:
    const FunctionType doLater;
};
