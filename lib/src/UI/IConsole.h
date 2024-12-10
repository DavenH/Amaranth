#pragma once

#include "../App/SingletonAccessor.h"
#include "JuceHeader.h"

class MouseUsage {
public:
	bool left;
	bool scroll;
	bool middle;
	bool right;

	MouseUsage() : left(0), scroll(0), middle(0), right(0) {}
	MouseUsage(bool l, bool r = false, bool s = false, bool m = false) : left(l), scroll(s), middle(m), right(r) {}
};

class IConsole : public SingletonAccessor {
public:
	enum {
	    DefaultPriority,
	    LowPriority,
	    ImportantPriority,
	    CriticalPriority
	};

	void write(const String& str) { write(str, DefaultPriority); }

	IConsole(SingletonRepo* repo) : SingletonAccessor(repo, "IConsole") {}

	virtual void write(const String& str, int priority) = 0;
	virtual void setKeys(const String& str) = 0;
	virtual void setMouseUsage(const MouseUsage& usage) = 0;
	virtual void reset() = 0;

    virtual void setMouseUsage(bool left, bool right, bool scroll, bool middle) {
        setMouseUsage(MouseUsage(left, right, scroll, middle));
    }

    void updateAll(const String& keys, const String& message, const MouseUsage& usage) {
		setKeys(keys);
		setMouseUsage(usage);
		write(message);
	}
};

class DummyConsole : public IConsole {
	DummyConsole(SingletonRepo* repo) : IConsole(repo) {}

	void write(const String& str, int priority) {}
	void setKeys(const String& str) {}
	void setMouseUsage(const MouseUsage& usage) {}
	void reset() {}
};