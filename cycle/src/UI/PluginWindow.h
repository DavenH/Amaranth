#include <Definitions.h>

#if PLUGIN_MODE

#ifndef _plugineditor_h
#define _plugineditor_h

#include "JuceHeader.h"
#include <PluginCharacteristics.h>
#include <Obj/Ref.h>
#include <App/SingletonAccessor.h>

class PluginProcessor;

class PluginWindow  :
		public AudioProcessorEditor
	,	public SingletonAccessor
{
public:
    PluginWindow (PluginProcessor* ownerFilter);
    ~PluginWindow();

    void paint (Graphics& g);
    void resized();
    void focusLost (FocusChangeType cause);
    void focusGained (FocusChangeType cause);
    void changeSizeAndSet(int sizeEnum);
    void moved();
    void freeUIResources();

private:
    bool haveFreedResources;
    bool doUpdateAfterResize;

    ScopedPointer<ResizableCornerComponent> resizer;
    ComponentBoundsConstrainer 				resizeLimits;
};


#endif

#endif
