#pragma once
#include <Definitions.h>
#include "../Incl/PluginCharacteristics.h"

#if PLUGIN_MODE

#include "JuceHeader.h"
#include <App/SingletonAccessor.h>

class PluginProcessor;

class PluginWindow  :
		public AudioProcessorEditor
	,	public SingletonAccessor
{
public:
    explicit PluginWindow (PluginProcessor* ownerFilter);
    ~PluginWindow() override;

    void paint (Graphics& g) override;
    void resized() override;
    void focusLost (FocusChangeType cause) override;
    void focusGained (FocusChangeType cause) override;
    void changeSizeAndSet(int sizeEnum);
    void moved() override;
    void freeUIResources();

private:
    bool haveFreedResources;
    bool doUpdateAfterResize;

    std::unique_ptr<ResizableCornerComponent> resizer;
    ComponentBoundsConstrainer 				resizeLimits;
};

#endif
