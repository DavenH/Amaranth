#ifndef THREADCHAIN_H_
#define THREADCHAIN_H_

#include "JuceHeader.h"

#include <iostream>

using std::cout;
using std::endl;

template<class Clazz>
class FunctionThread : public Thread {
public:
	FunctionThread(int waitMillisPerIter, int num, Clazz& clazz, void(Clazz::*func)()) :
			Thread("FunctionThread_" + String(num))
		,	waitMillis(waitMillisPerIter)
		,	recursions(num)
		, 	instance(clazz)
		, 	child(nullptr)
		, 	func(func) {
	}

    virtual ~FunctionThread() {
		cout << "Exiting thread" << getThreadName() << "\n";
	}

	void run() {
        if (recursions > 0) {
            wait(waitMillis);

			child = new FunctionThread(waitMillis, recursions - 1, instance, func);
			child->startThread();

			stopThread(10);

			delete this;
			return;
        } else {
            wait(waitMillis);
			(instance.*func)();
			stopThread(0);

			delete this;
		}
	}

private:
	std::unique_ptr<FunctionThread> child;
	void(Clazz::*func)(void);
	Clazz& instance;
	int recursions;
	int waitMillis;

	JUCE_LEAK_DETECTOR(FunctionThread);
};

class ThreadChain
{
public:
    ThreadChain();
    virtual ~ThreadChain();

    template<class Clazz>
    void runFunction(int waitMillis, Clazz& clazz, void(Clazz::*func)(void)) {
        FunctionThread<Clazz>* thread = new FunctionThread<Clazz>(waitMillis, 5, clazz, func);
		thread->startThread();
	}
};

#endif
