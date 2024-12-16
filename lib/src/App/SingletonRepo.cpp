#include <memory>

#include "SingletonRepo.h"
#include "AppConstants.h"
#include "MemoryPool.h"
#include "DocumentLibrary.h"
#include "Doc/Document.h"

#include "../Definitions.h"
#include "../Algo/AutoModeller.h"
#include "../App/EditWatcher.h"
#include "../App/MeshLibrary.h"
#include "../App/Settings.h"
#include "../Audio/AudioHub.h"
#include "../Curve/PathRepo.h"
#include "../Design/Updating/Updater.h"
#include "../UI/Panels/Panel.h"
#include "../UI/MiscGraphics.h"
#include "../Util/LogRegions.h"
#include "../Util/DummyOutputStream.h"


SingletonRepo::SingletonRepo() :
		hasInstantiated	(false)
	,	hasInitialized	(false)
	,	debugStream		(nullptr)
	,	statStream		(nullptr)
	,	dummyStream		(nullptr)
	,	lastDebugTime	(0) {
	repo = this;
}

SingletonRepo::~SingletonRepo() {
	getObj(Settings).writePropertiesFile();
	getObj(DocumentLibrary).writeSettingsFile();

	if(hasInstantiated)
		clearSingletons();
}

void SingletonRepo::instantiate() {
	if(hasInstantiated) {
		return;
	}
	dummyStream = std::make_unique<DummyOutputStream>();

	Settings::ClientPaths paths;
	String appDir(File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName());

  #ifdef JUCE_WINDOWS
	paths.propertiesPath = appDir + String("/Amaranth Audio/Cycle/install.xml");
  #elif defined(__APPLE__)
	paths.propertiesPath = appDir + String("/Preferences/com.amaranthaudio.Cycle.xml");
  #else
	paths.propertiesPath = appDir + String("/");
  #endif

	add(new MiscGraphics	(this),  	   -500);
	add(new AudioHub		(this),  	   -300);
	add(new Settings		(this, paths), -200);
	add(new MeshLibrary		(this),  	   -100);
	add(new MemoryPool		(this),  	   -100);
	add(new Updater			(this),    		 -1);
	add(new LogRegions		(this),    		 -1);
	add(new Document		(this));
	add(new PathRepo		(this));
	add(new EditWatcher		(this));
	add(new DocumentLibrary	(this));

	hasInstantiated = true;
}

void SingletonRepo::init() {
	if(hasInitialized)
		return;

	hasInitialized = true;

    struct Comparator {
        static int compareElements(SingletonAccessor* a, SingletonAccessor* b) {
			return a->getInitOrder() - b->getInitOrder();
		}
	};

	Comparator c;
	objects.sort(c);

	ScopedLock sl(initLock);

    for (auto object : objects) {
		std::cout << object->getName() << std::endl;
		object->init();
	}

	auto& document = getObj(Document);

	for(auto saveSource : saveSources)
		document.registerSavable(saveSource);

  #ifdef RUN_UNIT_TESTS
	for(int i = 0; i < testables.size(); ++i)
		testables[i]->test();
  #endif
}

void SingletonRepo::add(SingletonAccessor* accessor, int order) {
	accessor->setInitOrder(order);
	objects.add(accessor);
	hashes.set(accessor->getName(), accessor);

	if(auto* savable = dynamic_cast<Savable*>(accessor)) {
		saveSources.add(savable);
	}

	if(auto* panel = dynamic_cast<Panel*>(accessor)) {
		panels.add(panel);
	}

	if(auto* rasterizer = dynamic_cast<MeshRasterizer*>(accessor)) {
		rasterizers.add(rasterizer);
	}
}

OutputStream& SingletonRepo::getDebugStream() {
  #ifdef JUCE_DEBUG
	if(debugStream == nullptr)
		return *dummyStream;

	int64 curr = Time::currentTimeMillis();

	if(curr - lastDebugTime > 50)
		*debugStream << "\n";

	lastDebugTime 	= curr;
	Thread* thread 	= Thread::getCurrentThread();
	String name 	= (thread == nullptr) ? "MainThrd" : thread->getThreadName().substring(0, 8);

	*debugStream << "[" << (curr & 0xfffff) << "][" << name << "] ";

	return *debugStream;
  #else
	return *dummyStream;
  #endif
}

OutputStream& SingletonRepo::getStatStream() {
    return statStream == nullptr ? *dummyStream : *statStream;
}

void SingletonRepo::resetSingletons() {
    for (auto object : objects)
        object->reset();
}

void SingletonRepo::clearSingletons() {
    objects.clear(true);
}

void SingletonRepo::setDeformer(IDeformer* deformer) {
	this->deformer = deformer;

	for(auto rasterizer : rasterizers)
		rasterizer->setDeformer(deformer);
}
