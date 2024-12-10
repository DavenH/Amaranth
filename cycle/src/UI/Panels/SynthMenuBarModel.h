#ifndef _synthmenubarmodel_h
#define _synthmenubarmodel_h

#include "JuceHeader.h"
#include <Obj/Ref.h>
#include <Definitions.h>
#include <App/SingletonAccessor.h>

class MiscGraphics;
class Dialogs;
class FileManager;
class SynthAudioSource;
class EditWatcher;

class EditableItem :
	public PopupMenu::CustomComponent,
	public Button::Listener
{
public:
	EditableItem(const String& name);
	void resized();
	void buttonClicked(Button* button);
	void getIdealSize (int& idealWidth, int& idealHeight);

private:
	TextButton saveButton;
	Label editor;

	String name;
};

class SynthMenuBarModel :
		public MenuBarModel
	,	public SingletonAccessor
{
public:
	SynthMenuBarModel(SingletonRepo* repo);

	virtual ~SynthMenuBarModel()
	{
	}

	enum Menus
	{
		FileMenu,
		EditMenu,
		GraphicsMenu,
		AudioMenu,
		HelpMenu
	};

	enum FileItem
	{
		NewFile = 1,
		OpenFile,
		SaveFile,
		SaveAsFile,
		RevertToSaved,
		LoadSample,
		LoadMultiSample,
		UnloadSample,
		SaveAudioMeta,

		ExitProgram
	};

	enum GraphicsItem
	{
		ShowSynth = 1,
		ShowBoth,
		ShowWave,

		VertsOnHover,
		VertsAlways,

		DepthCurrDim,
		DepthAllDim,
		DrawScales,

		ViewStageA,
		ViewStageB,
		ViewStageC,
		ViewStageD,

		WaveformWaterfall,

		UseOpenGL,
		UseLargerPoints,

		PointSize1x,
		PointSize2x,
		PointSize4x,

		Reduction1x,
		Reduction3x,
		Reduction5x,

		InterpWaveCycles,
		WrapWaveCycles
	};

	enum EditItem
	{
		UndoEdit = 1,
		RedoEdit,
		EraseVerts,
		ExtrudeVerts,

		AudioConfigEdit,
		SnapEdit,

		CollisionDetection,
		SelectWithRight,
		NativeDialogs,
		PitchOctaveUp,
		PitchOctaveDown,
		PitchAlgorithmAuto,
		PitchAlgorithmYin,
		PitchAlgorithmSwipe,

		UpdateRealtime,
		UpdateOnRelease,

		DebugLogging,
	};

	enum
	{
		NoteStart = 0x10000
	};

	enum AudioItem
	{
		QualityOptions = 1,
		ModMatrix,
		Declick,
		BendRange2,
		BendRange6,
		BendRange12,

//		MapNull,
//		MapMod,
//		MapAft,
//		MapVelInv,
//		MapVel,

		DynamicEnvs,

		OversampRltm1x,
		OversampRltm2x,
		OversampRltm4x,
		OversampRltm16x,
	};

	enum HelpItem
	{
		About = 1,
		Help,
		Tour,
		Cheatsheet,
		Tutorials = 10,
	};

	/** This method must return a list of the names of the menus. */
	StringArray getMenuBarNames();
	PopupMenu getMenuForIndex(int topLevelMenuIndex, const String& menuName);
	void menuItemSelected(int item, int topLevelMenuIndex);
	void triggerClick(int stage);
	void init();

private:
	JUCE_LEAK_DETECTOR(SynthMenuBarModel)

	Ref<MiscGraphics> 		miscGfx;
	Ref<Dialogs>			dialogs;
	Ref<FileManager>		fileMgr;
	Ref<SynthAudioSource> 	source;
	Ref<EditWatcher>		editWatcher;

	Array<File> tutorials;
};

#endif
