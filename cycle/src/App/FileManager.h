#ifndef PRESETMANAGER_H_
#define PRESETMANAGER_H_

#include "JuceHeader.h"
#include <App/SingletonAccessor.h>
#include "Dialogs.h"

class FileManager :
		public SingletonAccessor {
public:
	explicit FileManager(SingletonRepo* repo);
	~FileManager() override;

	void unloadWav(bool);

	void loadPendingItem();
	void saveCurrentPreset();
	void openCurrentPreset();
	void openDefaultPreset();
	void openFactoryPreset(const String& presetName);
	void openPreset(const File& file);
	void doPostPresetLoad();
	void revertCurrentPreset();
	bool canSaveOverCurrentPreset();

	bool openWave(const File& file, Dialogs::OpenWaveInvoker invoker, int defaultNote = -1);
	void doPostWaveLoad(Dialogs::OpenWaveInvoker invoker);

	//void launchCheatsheetDocument();
	//void launchHelp();
	//void launchTutorials();

	void setCurrentPresetName(const String& str) { currentPresetName = str;			 }
	const String& getCurrentPresetName()		 { return currentPresetName; 		 }
	void setOpenDefaultPreset(bool should) 		 { shouldOpenDefaultPreset = should; }

	static void revertCallback(int returnId, SingletonRepo* repo);
private:
	bool shouldOpenDefaultPreset;

	String currentPresetName;
	String defaultPresetName;
};

#endif
