#pragma once

#include <map>
#include "JuceHeader.h"
#include "../App/SingletonAccessor.h"
#include "../Util/MicroTimer.h"

class OpenGLBase;

class MasterRenderer :
	public OpenGLContext::OpenGLThread,
	public SingletonAccessor {
public:
	MasterRenderer(SingletonRepo* repo);

	~MasterRenderer() override;
	void repaintAll();

	void init();
	void run();
	void removeContext(OpenGLBase* panel, const String& name = String());
	void removeContext(OpenGLContext* context);
	void addContext(OpenGLBase* panel, const String& name = String());
	void addContext(OpenGLContext* context);

	void contextDestroyed(OpenGLContext* context);
	void contextCreated(OpenGLContext* context);

	CriticalSection& getRenderLock() { return renderLock; }
private:
	CriticalSection arrayLock, renderLock;
	Array<OpenGLContext*> contexts, contextsToRemove, contextsToAdd;

//  #ifdef _DEBUG
	std::map<OpenGLContext*, String> contextNames;
//  #endif

	MicroTimer timer;
};