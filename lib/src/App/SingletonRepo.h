#pragma once

#include "JuceHeader.h"
#include "../Obj/Ref.h"

class Savable;
class PluginProcessor;
class SingletonAccessor;
class MorphPositioner;
class IConsole;
class GuideCurveProvider;
class Panel;

using namespace juce;

class SingletonRepo {
public:
    SingletonRepo();
    virtual ~SingletonRepo();

    void init();
    void instantiate();
    void add(SingletonAccessor* client, int order = 0);
    void addExternal(SingletonAccessor* client, int order = 0);
    void clearSingletons();
    void resetSingletons();
    void resizeVertices();

    OutputStream& getDebugStream();
    OutputStream& getStatStream();

    void setPluginProcessor(PluginProcessor* proc)      { plugin = proc;                     }
    void setDebugStream(OutputStream* output)           { this->debugStream.reset(output);   }
    void setMorphPositioner(MorphPositioner* positioner){ this->positioner = positioner;     }
    void setConsole(IConsole* console)                  { this->console = console;           }
    void setSuppressAudioDeviceInit(bool shouldSuppress){ suppressAudioDeviceInit = shouldSuppress; }
    void setSuppressSavableAutoRegistration(bool shouldSuppress) { suppressSavableAutoRegistration = shouldSuppress; }
    void setSuppressInitializerInit(bool shouldSuppress) { suppressInitializerInit = shouldSuppress; }
    void setGuideCurveProvider(GuideCurveProvider* guideCurveProvider);

    PluginProcessor& getPluginProcessor()               { jassert(plugin != nullptr); return *plugin; }
    MorphPositioner& getMorphPosition()                 { return *positioner;   }
    IConsole& getConsole()                              { return *console;      }
    GuideCurveProvider* getGuideCurveProviderPtr() const { return guideCurveProvider.get(); }
    bool shouldSuppressAudioDeviceInit() const          { return suppressAudioDeviceInit; }
    bool shouldSuppressSavableAutoRegistration() const  { return suppressSavableAutoRegistration; }
    bool shouldSuppressInitializerInit() const          { return suppressInitializerInit; }

    /* ----------------------------------------------------------------------------- */

    template<class T>
    T& get(const String& name)
    {
        jassert(hashes.contains(name));

        return *dynamic_cast<T*>(hashes[name]);
    }

    template<class T>
    const T& get(const String& name) const
    {
        jassert(hashes.contains(name));

        return *dynamic_cast<T*>(hashes[name]);
    }

    CriticalSection& getInitLock() { return initLock; }

protected:
    bool hasInstantiated, hasInitialized;
    bool suppressAudioDeviceInit;
    bool suppressSavableAutoRegistration;
    bool suppressInitializerInit;
    int64 lastDebugTime;

    SingletonRepo* repo;

    Ref<PluginProcessor> plugin;

    std::unique_ptr<OutputStream> dummyStream;
    std::unique_ptr<OutputStream> debugStream;
    std::unique_ptr<OutputStream> statStream;

    Ref<MorphPositioner> positioner;
    Ref<IConsole> console;
    Ref<GuideCurveProvider> guideCurveProvider;

    Array<Savable*> saveSources;
    Array<Panel*> panels;

    HashMap<String, SingletonAccessor*> hashes;
    OwnedArray<SingletonAccessor> objects;

    CriticalSection initLock;
};

template<>
PluginProcessor& SingletonRepo::get<PluginProcessor>(const String& name);

template<>
const PluginProcessor& SingletonRepo::get<PluginProcessor>(const String& name) const;
