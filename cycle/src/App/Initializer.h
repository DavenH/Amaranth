#pragma once

#include <App/SingletonAccessor.h>
#include "JuceHeader.h"

#include "EnvRasterizerServices.h"

class SingletonRepo;
class FXRasterizer;
class Panel;

class Initializer : public SingletonAccessor
{
public:
    Initializer();
    ~Initializer() override;

    void init() override;
    void doPostInitWiring();
    void initSingletons();
    void resetAll(bool performUpdate = true);
    void setConstants();
    void setDefaultSettings();
    void instantiate();
    void seedMeshLibrary();
    void freeUIResources();

    int getInstanceId() const 					{ return instanceId; }
    void setCommandLine(const String& command) 	{ this->commandLine = command; }
    const String& getCommandLine() 				{ return commandLine; }

    void takeLocks();
    void releaseLocks();

private:
    bool haveInitDsp;
    bool haveInitGfx;
    bool haveFreedResources;
    int instanceId;

    String commandLine;
    String dllName;

    std::unique_ptr<SingletonRepo> singletonRepo;
    Array<Panel*> lockingPanels;

    static Atomic<int> numInstances;
};

typedef FXRasterizer  EnvWavePitchRast;
