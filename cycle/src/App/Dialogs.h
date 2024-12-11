#ifndef _Dialogs_h
#define _Dialogs_h

#include <vector>
#include <stack>

#include <App/Doc/DocumentDetails.h>
#include <App/Doc/Savable.h>
#include <Audio/SampleWrapper.h>
#include <Inter/Dimensions.h>
#include <Obj/Ref.h>
#include "JuceHeader.h"
#include <Util/StatusChecker.h>
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


	Dialogs(SingletonRepo* repo);
	virtual ~Dialogs();

#if ! PLUGIN_MODE
	void showAudioSettings();
#endif

	void init();
	void launchHelp();
	bool promptForSaveModally();
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
	void timerCallback();
	void addPendingAction(PostModalAction action) { pendingModalActions.push(action); }
	void showOpenWaveDialog(SampleWrapper* dstWav, const String& subDir, int actionContext,
							PostModalAction postAction = DoNothing, bool forDirectory = false);

    static void getSizeFromSetting(int sizeEnum, int& width, int& height);

	struct WaveOpenData
	{
		int actionContext;
		File audioFile;
		Dialogs* ops;
		SampleWrapper* wrapper;
		PostModalAction postAction;

		WaveOpenData(Dialogs* ops, SampleWrapper* wrapper,
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
	void createAlertSaveWindow();
	String getPresetSaveAction();

	Ref<MainPanel> mainPanel;
	Ref<EditWatcher> watcher;
	stack<PostModalAction> pendingModalActions;

	std::unique_ptr<WildcardFileFilter> 	fileFilter;
	std::unique_ptr<FileChooserDialogBox> fileLoader;
	std::unique_ptr<FileBrowserComponent> fileBrowser;
	std::unique_ptr<FileChooser> 			nativeFileChooser;
};

#endif

