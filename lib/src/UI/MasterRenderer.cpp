#include "MasterRenderer.h"
#include "Panels/OpenGLBase.h"
#include "../App/SingletonRepo.h"
#include "../Definitions.h"
#include "../Util/Util.h"

MasterRenderer::MasterRenderer(SingletonRepo* repo) :
		OpenGLThread("OpenGlThread")
	,	SingletonAccessor(repo, "MasterRenderer") {
}

MasterRenderer::~MasterRenderer() {
    stopThread(100);
}

void MasterRenderer::init() {
    startThread(6);
}

void MasterRenderer::repaintAll() {
	for(int i = 0; i < contexts.size(); ++i)
		contexts[i]->triggerRepaint();
}

void MasterRenderer::run() {
    bool failedNone = true;

    while (!threadShouldExit()) {
        Array < OpenGLContext * > toRemove, toAdd;

        {
			ScopedLock sl(arrayLock);

			toRemove = contextsToRemove;
			toAdd 	 = contextsToAdd;

			contextsToRemove.clear();
			contextsToAdd.clear();
		}

        if (toAdd.size() > 0) {
            for (int i = 0; i < toAdd.size(); ++i) {
				OpenGLContext* context = toAdd[i];

				context->setRenderThread(this);
				contexts.addIfNotAlreadyThere(context);
			}
		}

        {
            failedNone = true;

            // check for dying contexts first as main thread will be waiting
            for (int i = 0; i < contexts.size(); ++i) {
                OpenGLContext* context = contexts[i];
				if(context->getNeedsDestroying()) {
					if(! context->isActive()) {
						context->makeActive();
					}

					context->destroy();
				}
			}

            for (int i = 0; i < contexts.size(); ++i) {
                OpenGLContext* context = contexts[i];

                if (context->getNeedsInitialising())
					context->initialise();

				failedNone &= context->renderFrameIfDirty();
			}

			contexts.removeValuesIn(toRemove);
		}

		wait(failedNone ? -1 : 4);
	}
}

void MasterRenderer::addContext(OpenGLContext* context) {
    ScopedLock sl(arrayLock);

    contextsToAdd.addIfNotAlreadyThere(context);
    notify();
}

void MasterRenderer::addContext(OpenGLBase* panel, const String& name) {
    OpenGLContext* context = &panel->context;

	contextNames[context] = name;

	if(contexts.contains(context))
		return;

	cout << name << ": adding context\n";

	addContext(context);
}

void MasterRenderer::removeContext(OpenGLBase* panel, const String& name) {
    OpenGLContext* context = &panel->context;
    contextNames[context] = name;

    if (!contexts.contains(context)) {
	    return;
    }

	std::cout << name << ": removing context\n";

	removeContext(context);
}

void MasterRenderer::removeContext(OpenGLContext* context) {
    ScopedLock sl(arrayLock);

    contextsToRemove.addIfNotAlreadyThere(context);
    notify();
}

void MasterRenderer::contextDestroyed(OpenGLContext* context) {
  #ifdef _DEBUG
	std::cout << contextNames[context] << " destroying context [implicit]\n";
  #endif

	context->setNeedsDestroying(true);
	notify();
}

void MasterRenderer::contextCreated(OpenGLContext* context) {
  #ifdef _DEBUG
	dout << contextNames[context] << " initializing context [implicit]\n";
  #endif
	context->setNeedsInitialising(true);
	notify();
}
