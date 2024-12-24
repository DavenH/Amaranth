#pragma once

#include "JuceHeader.h"
#include "../Obj/Ref.h"

class Savable;
class PluginProcessor;
class SingletonAccessor;
class MeshRasterizer;
class MorphPositioner;
class IConsole;
class IDeformer;
class Panel;
using namespace juce;

class SingletonRepo {
public:
    SingletonRepo();
    virtual ~SingletonRepo();

    void init();
    void instantiate();
    void add(SingletonAccessor* client, int order = 0);
    void clearSingletons();
    void resetSingletons();

    OutputStream& getDebugStream();
    OutputStream& getStatStream();

    void setPluginProcessor(PluginProcessor* proc)      { plugin = proc;                     }
    void setDebugStream(OutputStream* output)           { this->debugStream.reset(output);   }
    void setMorphPositioner(MorphPositioner* positioner){ this->positioner = positioner;     }
    void setConsole(IConsole* console)                  { this->console = console;           }
    void setDeformer(IDeformer* deformer);

    MorphPositioner& getMorphPosition()                 { return *positioner;   }
    IConsole& getConsole()                              { return *console;      }
    IDeformer& getDeformer()                            { return *deformer;     }

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
    int64 lastDebugTime;

    SingletonRepo* repo;

    Ref<PluginProcessor> plugin;

    std::unique_ptr<OutputStream> dummyStream;
    std::unique_ptr<OutputStream> debugStream;
    std::unique_ptr<OutputStream> statStream;

    Ref<MorphPositioner> positioner;
    Ref<IConsole> console;
    Ref<IDeformer> deformer;

    Array<Savable*> saveSources;
    Array<Panel*> panels;
    Array<MeshRasterizer*> rasterizers;

    HashMap<String, SingletonAccessor*> hashes;
    OwnedArray<SingletonAccessor> objects;

    CriticalSection initLock;
};
