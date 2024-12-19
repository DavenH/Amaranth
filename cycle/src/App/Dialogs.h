#pragma once

#include <vector>
#include <stack>

#include <Obj/Ref.h>
#include "JuceHeader.h"
#include "../UI/Dialogs/QualityDialog.h"

using std::vector;
using std::stack;

class MainPanel;
class EditWatcher;

class Dialogs :
		public SingletonAccessor
	,	public Timer
{
public:
	enum DialogSaveId	 { DialogCancel, DialogSave, DialogDontSave };
	enum OpenWaveInvoker { PresetSource, DialogSource };

	enum PostModalAction
	{
		DoNothing
	,	Shutdown
	,	LoadPresetItem
	,	LoadEmptyPreset
	,	ShowOpenPresetDialog
	,	ShowSavePresetDialog
	,	SavePreset
	,	ModelIRWave
	,	LoadIRWave
	,	LoadMultisample
	,
	};

	enum PitchAlgos
	{
		AlgoYin = 1,
		AlgoSwipe
	};


	explicit Dialogs(SingletonRepo* repo);

	~Dialogs() override = default;

  #if ! PLUGIN_MODE
	void showAudioSettings();
  #endif

	void init() override;
	void launchHelp();
	void promptForSaveModally(const std::function<void(bool)>& completionCallback);
	void promptForSaveApplicably(PostModalAction action);
    void handleNextPendingModalAction();
	void showAboutDialog();
	void showDebugPopup();
	void showDeviceErrorApplicably();
	void showModMatrix();
	void showOpenPresetDialog();
	void showOutputPopup();
	void showPresetBrowserModal();
	void showPresetSaveAsDialog();
	void showQualityOptions();
	void showSamplePlacer();
	void timerCallback() override;
	void addPendingAction(PostModalAction action) { pendingModalActions.push(action); }
	void showOpenWaveDialog(PitchedSample* dstWav, const String& subDir, int actionContext,
							PostModalAction postAction = DoNothing, bool forDirectory = false);

    static void getSizeFromSetting(int sizeEnum, int& width, int& height);

	struct WaveOpenData
	{
		int actionContext;
		File audioFile;
		Dialogs* ops;
		PitchedSample* wrapper;
		PostModalAction postAction;

		WaveOpenData(Dialogs* ops, PitchedSample* wrapper,
					 int actionContext, PostModalAction postAction) :
				ops(ops)
			,	wrapper(wrapper)
			,	actionContext(actionContext)
			,	postAction(postAction)
		{
		}

	};

	static void saveCallback(int returnId, SingletonRepo* repo);
	static void openWaveCallback(int returnId, WaveOpenData ops);
	static void openPresetCallback(int returnId, const File& currentFile, Dialogs* ops);

private:
	bool handleSaveAction(int menuResult);
	void createAlertSaveWindow();
	String getPresetSaveAction();

	Ref<MainPanel> mainPanel;
	Ref<EditWatcher> watcher;
	stack<PostModalAction> pendingModalActions;

	std::unique_ptr<WildcardFileFilter> fileFilter;
	std::unique_ptr<FileChooserDialogBox> fileLoader;
	std::unique_ptr<FileBrowserComponent> fileBrowser;
	std::unique_ptr<FileChooser> nativeFileChooser;
};
