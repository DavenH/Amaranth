#ifndef INITIALIZER_H_
#define INITIALIZER_H_

#include <App/SingletonAccessor.h>
#include "JuceHeader.h"

class SingletonRepo;
class EnvRasterizer;
class FXRasterizer;
class Panel;

class Initializer : public SingletonAccessor
{
public:
	Initializer();
	virtual ~Initializer();

	void init();
	void init2();
	void initSingletons();
	void resetAll();
	void setConstants();
	void setDefaultSettings();
	void instantiate();
//	void updateDllName();
	void freeUIResources();

	int getInstanceId() 						{ return instanceId; }
	void setCommandLine(const String& command) 	{ this->commandLine = command; }
	const String& getCommandLine() 				{ return commandLine; }

	void takeLocks(SingletonRepo* repo);
	void releaseLocks(SingletonRepo* repo);

private:
	bool haveInitDsp;
	bool haveInitGfx;
	bool haveFreedResources;
	int instanceId;

	String commandLine;
	String dllName;

	ScopedPointer<SingletonRepo> singletonRepo;
	Array<Panel> lockingPanels;

	static Atomic<int> numInstances;
};

typedef EnvRasterizer EnvVolumeRast;
typedef EnvRasterizer EnvPitchRast;
typedef EnvRasterizer EnvScratchRast;
typedef FXRasterizer  EnvWavePitchRast;

#endif
