#pragma once
#include <utility>

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
    std::function<void()> laterFn{};
};
