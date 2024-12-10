#ifndef LOCKREGISTRY_H_
#define LOCKREGISTRY_H_

#include <vector>
#include <map>
#include "../App/SingletonAccessor.h"
#include "JuceHeader.h"

using std::map;
using std::vector;

class LockRegistry :
	public SingletonAccessor {
public:
	LockRegistry(SingletonRepo* repo);
	virtual ~LockRegistry();

	void lockEntered(void* address);
	void lockExited(void* address);

	class LockEvent {
	public:
		int lockId;
		int threadId;
		bool isEnter;
	};
private:
	map<int, int> entries;
	map<int, int> exits;

	vector<LockEvent> events;
};

#endif
