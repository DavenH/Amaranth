#pragma once
#include "JuceHeader.h"
#include "UpdateType.h"

using namespace juce;

class Updateable {
public:
    virtual ~Updateable() = default;

    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void preUpdateHook(int updateType) {}
        virtual void postUpdateHook(int updateType) {}
    };

    void update(UpdateType updateType) {
        listeners.call(&Listener::preUpdateHook, updateType);
        performUpdate(updateType);
        listeners.call(&Listener::postUpdateHook, updateType);
    }

    virtual void performUpdate(UpdateType updateType) = 0;

    String getUpdateName() { return updateName; }
    void setUpdateName(const String& name) { updateName = name; }
    void addListener(Listener* listener) { listeners.add(listener); }

protected:
    ListenerList<Listener> listeners;
    String updateName;
};
