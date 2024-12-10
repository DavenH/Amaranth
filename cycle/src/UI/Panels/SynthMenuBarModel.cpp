#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/PluginProcessor.h>
#include <Definitions.h>
#include <Design/Updating/Updater.h>
#include <Inter/Interactor.h>
#include <Inter/Interactor3D.h>
#include <Inter/UndoableMeshProcess.h>
#include <UI/IConsole.h>
#include <UI/MiscGraphics.h>
#include <Util/Util.h>
#include <Util/CommonEnums.h>

#include "GeneralControls.h"
#include "SynthMenuBarModel.h"
#include "VertexPropertiesPanel.h"

#include "../Dialogs/QualityDialog.h"
#include "../Panels/MainPanel.h"
#include "../App/CycleTour.h"
#include "../VertexPanels/Waveform2D.h"

#include "../../App/MainAppWindow.h"
#include "../../App/Dialogs.h"
#include "../../App/Directories.h"
#include "../../App/FileManager.h"
#include "../../Audio/SampleUtils.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../CycleDefs.h"
#include "../../UI/VisualDsp.h"
#include "../../Util/CycleEnums.h"


SynthMenuBarModel::SynthMenuBarModel(SingletonRepo* repo) : SingletonAccessor(repo, "SynthMenuBarModel")
{
}

/** This method must return a list of the names of the menus. */
StringArray SynthMenuBarModel::getMenuBarNames()
{
	StringArray array;

	array.add("File");
	array.add("Edit");
	array.add("Graphics");
	array.add("Audio");
	array.add("Help");

	return array;
}

/** This should return the popup menu to display for a given top-level menu.

 @param topLevelMenuIndex	the index of the top-level menu to show
 @param menuName		 the name of the top-level menu item to show
 */
PopupMenu SynthMenuBarModel::getMenuForIndex(int topLevelMenuIndex, const String& menuName)
{
	getSetting(LastPopupClickedHorz) 	= false;
	getSetting(LastPopupClickedTransp)		= false;

	PopupMenu menu;

	if(topLevelMenuIndex == FileMenu)
	{
		bool haveEdited = getObj(EditWatcher).getHaveEdited();
		bool canSaveAs = true;

	  #ifdef DEMO_VERSION
		haveEdited = false;
		canSaveAs = false;
	  #endif

		menu.addItem(NewFile, 		"New");
		menu.addItem(OpenFile, 		"Open");

		menu.addSeparator();
		menu.addItem(SaveFile, 		"Save", 		haveEdited);
		menu.addItem(SaveAsFile, 	"Save as...", 	canSaveAs);

		menu.addSeparator();
		menu.addItem(RevertToSaved, "Revert to saved");

		menu.addSeparator();
	  #ifndef BEAT_EDITION
		menu.addItem(LoadSample, 	"Load reference sample...");
		menu.addItem(LoadMultiSample,"Load multiSample...");
		menu.addItem(UnloadSample, 	"Unload sample");
	  #endif

		menu.addSeparator();
	  #if ! PLUGIN_MODE
		menu.addItem(ExitProgram, 	"Exit", 			true, 		false);
	  #endif
	}

	else if(topLevelMenuIndex == EditMenu)
	{
		UndoManager& undoManager = editWatcher->getUndoManager();

		bool canUndo = undoManager.canUndo();
		bool canRedo = undoManager.canRedo();

		const String& undoString = "Undo" + (canUndo ? " (" + undoManager.getUndoDescription() + ")" : String::empty);
		const String& redoString = "Redo" + (canRedo ? " (" + undoManager.getRedoDescription() + ")" : String::empty);

		menu.addItem(UndoEdit, 	undoString, 	canUndo, 	false);
		menu.addItem(RedoEdit, 	redoString, 	canRedo,	false);
		menu.addSeparator();

		Interactor* itr 	= getObj(VertexPropertiesPanel).getCurrentInteractor();
		bool haveSelected 	= itr != nullptr && ! itr->getSelected().empty();
		bool canExtrude 	= itr != nullptr && itr->is3DInteractor();

		menu.addItem(EraseVerts, 	"Erase selected (del)", haveSelected,	false);
		menu.addItem(ExtrudeVerts, 	"Extrude selected (e)", canExtrude,		false);
		menu.addSeparator();

	  #if ! PLUGIN_MODE
		menu.addItem(AudioConfigEdit, 	"Audio Settings", 	true, 	false);
	  #endif

		menu.addSeparator();
		menu.addItem(SnapEdit, 			 "Snap to grid",			true, getSetting(SnapMode) == 1);
		menu.addItem(CollisionDetection, "Prevent line collisions",	true, getSetting(CollisionDetection) == 1);

		PopupMenu updateMenu;
		updateMenu.addItem(UpdateRealtime, 	"All events",			true, getSetting(UpdateGfxRealtime) == 1);
		updateMenu.addItem(UpdateOnRelease, "Drag release",			true, getSetting(UpdateGfxRealtime) != 1);

		menu.addSubMenu("Update On", updateMenu);

		menu.addSeparator();

		menu.addItem(SelectWithRight, 	"Select with right click", 	true, getSetting(SelectWithRight) == 1);
		menu.addItem(NativeDialogs, 	"Use native dialogs", 		true, getSetting(NativeDialogs) == 1);
		menu.addSeparator();

	  #ifndef BEAT_EDITION
		PopupMenu pitchMenu;

		PopupMenu algoMenu;
		algoMenu.addItem(PitchAlgorithmAuto, 	"Auto",  true, getSetting(PitchAlgo) == PitchAlgos::AlgoAuto );
		algoMenu.addItem(PitchAlgorithmYin, 	"YIN", 	 true, getSetting(PitchAlgo) == PitchAlgos::AlgoYin  );
		algoMenu.addItem(PitchAlgorithmSwipe, 	"Swipe", true, getSetting(PitchAlgo) == PitchAlgos::AlgoSwipe);

		PopupMenu setPitchMenu;

		setPitchMenu.addItem(PitchOctaveDown, 	"Octave Down");
		setPitchMenu.addItem(PitchOctaveUp, 	"Octave Up");

		pitchMenu.addSubMenu("Tracking Algo", algoMenu);
		pitchMenu.addSubMenu("Set Pitch", setPitchMenu);

		menu.addSubMenu("Pitch Tracking", pitchMenu);
	  #endif
	}

	else if(topLevelMenuIndex == GraphicsMenu)
	{
		menu.addItem(WaveformWaterfall, "Waveform waterfall", 		true, getSetting(Waterfall) == 1);
		menu.addItem(DrawScales, 		"Draw scales and tags", 	true, getSetting(DrawScales) == 1);
		menu.addItem(VertsOnHover, 		"Draw verts only on hover",	true, getSetting(ViewVertsOnlyOnHover) == 1);

		menu.addSeparator();

		{
			int viewStage = getSetting(ViewStage);
			PopupMenu waveformMenu;
			waveformMenu.addItem(ViewStageA, "1. Just wireframe", getSetting(DrawWave) == 0, viewStage == ViewStages::PreProcessing);
			waveformMenu.addItem(ViewStageB, "2. After envelopes", 			true, viewStage == ViewStages::PostEnvelopes);
			waveformMenu.addItem(ViewStageC, "3. After spectral filtering", true, viewStage == ViewStages::PostSpectrum);
			waveformMenu.addItem(ViewStageD, "4. After effects", 			true, viewStage == ViewStages::PostFX);
			menu.addSubMenu("Displayed Processing Stage", waveformMenu, true);
		}


		{
			PopupMenu sizeMenu;
			sizeMenu.addItem(PointSize1x, "1x", true, getSetting(PointSizeScale) == ScaleSizes::ScaleSmall);
			sizeMenu.addItem(PointSize2x, "2x", true, getSetting(PointSizeScale) == ScaleSizes::ScaleMed);
			sizeMenu.addItem(PointSize4x, "4x", true, getSetting(PointSizeScale) == ScaleSizes::ScaleLarge);

			menu.addSubMenu("Vertex Point Size", sizeMenu);
		}

		{
			int factor = getSetting(ReductionFactor);

			PopupMenu reductMenu;
			reductMenu.addItem(Reduction1x, "1x", true, factor == 1);
			reductMenu.addItem(Reduction3x, "3x", true, factor == 3);
			reductMenu.addItem(Reduction5x, "5x", true, factor == 5);

			menu.addSubMenu("Detail Reduction", reductMenu);
		}


	  #ifndef BEAT_EDITION
		PopupMenu sampleMenu;

		sampleMenu.addItem(InterpWaveCycles, 	"Interpolate", true, getSetting(InterpWaveCycles) == 1);
		sampleMenu.addItem(WrapWaveCycles, 		"Wrap cycles", true, getSetting(WrapWaveCycles) == 1);
		menu.addSubMenu("Sample Display", sampleMenu, true);
	  #endif
	}

	else if(topLevelMenuIndex == AudioMenu)
	{
		menu.addItem(Declick, "Declick", true, getDocSetting(Declick) == 1);

	  #ifndef BEAT_EDITION
//		menu.addItem(DynamicEnvs, "Dynamic Envelopes", true, getDocSetting(DynamicEnvelopes) == 1);
	  #endif
		int bendRange = getDocSetting(PitchBendRange);
		menu.addSeparator();

		PopupMenu bendMenu;
		bendMenu.addItem(BendRange2, 	"+- 2",		true, bendRange == 2);
		bendMenu.addItem(BendRange6, 	"+- 6",		true, bendRange == 6);
		bendMenu.addItem(BendRange12, 	"+- 12",	true, bendRange == 12);

		menu.addSubMenu("Pitch Bend Range", bendMenu, true, Image::null, false);

	  #ifndef BEAT_EDITION
		menu.addItem(ModMatrix, 		"Modulation Matrix...");
		menu.addItem(QualityOptions, 	"Quality options...");
	  #else
		PopupMenu oversampMenu;
		oversampMenu.addItem(OversampRltm1x, "None", true);
		oversampMenu.addItem(OversampRltm2x, "2x", true);
		oversampMenu.addItem(OversampRltm4x, "4x", true);
		menu.addSubMenu("Oversample", oversampMenu, true);
	  #endif

	}

	else if(topLevelMenuIndex == HelpMenu)
	{
		menu.addSeparator();

		PopupMenu tutMenu;

		for(int i = 0; i < tutorials.size(); ++i)
		{
			const File& file = tutorials.getReference(i);
			XmlDocument doc(file);
			ScopedPointer<XmlElement> elem = doc.getDocumentElement(true);

			String name = elem->getStringAttribute("title", file.getFileNameWithoutExtension());
			tutMenu.addItem(Tutorials + i, name, true, false);
		}

		menu.addSubMenu("Tutorials", tutMenu, true);

	  #ifndef BEAT_EDITION
		menu.addItem(Help, 				"Online Help");
		//menu.addItem(Cheatsheet, 		"Cheatsheet");
	  #endif
		menu.addItem(About, 			"About");
	}

	return menu;
}

/** This is called when a menu item has been clicked on.

 @param menuItemID	   the item ID of the PopupMenu item that was selected
 @param topLevelMenuIndex	the index of the top-level menu from which the item was
 chosen (just in case you've used duplicate ID numbers
 on more than one of the popup menus)
 */
void SynthMenuBarModel::menuItemSelected(int item, int topLevelMenuIndex)
{
	bool editedSomething = false;

	if (topLevelMenuIndex == FileMenu)
	{
		if(item == NewFile)
		{
			dialogs->promptForSaveApplicably(Dialogs::LoadEmptyPreset);
		}
		else if(item == OpenFile)
		{
			dialogs->promptForSaveApplicably(Dialogs::ShowOpenPresetDialog);
		}
		else if(item == SaveFile) 		fileMgr->saveCurrentPreset();
		else if(item == SaveAsFile)		dialogs->showPresetSaveAsDialog();
		else if(item == RevertToSaved)	fileMgr->revertCurrentPreset();

		else if(item == LoadSample)
		{
			dialogs->showOpenWaveDialog(nullptr, String::empty, DialogActions::TrackPitchAction);
		}

		else if(item == LoadMultiSample)
		{
			dialogs->showOpenWaveDialog(nullptr, String::empty, Dialogs::LoadMultisample,
			                            Dialogs::DoNothing, true);
		}

		else if (item == UnloadSample)
		{
			if (! getSetting(WaveLoaded))
			{
				showMsg("No wave file loaded!");
				return;
			}

			getObj(SampleUtils).unloadWav();

			getSetting(DrawWave) 	= false;
			getSetting(WaveLoaded) 	= false;

			getObj(GeneralControls).updateHighlights();

			getObj(Updater).update(UpdateSources::SourceAllButFX, (int) UpdateType::Repaint);
		}

		else if (item == SaveAudioMeta)
		{
//			fileMgr->saveWavePitchEnvelope();
		}

	  #if ! PLUGIN_MODE
		else if(item == ExitProgram)
		{
			getObj(MainAppWindow).closeButtonPressed();
		}
	  #endif
	}

	else if(topLevelMenuIndex == EditMenu)
	{
		if(item == UndoEdit)		editWatcher->undo();
		else if(item == RedoEdit)	editWatcher->redo();

	  #if ! PLUGIN_MODE
		else if(item == AudioConfigEdit)	dialogs->showAudioSettings();
	  #endif

		else if(item == EraseVerts)
		{
			Interactor* itr = getObj(VertexPropertiesPanel).getCurrentInteractor();
			if(itr != nullptr)
				itr->eraseSelected();
		}

		else if(item == ExtrudeVerts)
		{
			Interactor* itr = getObj(VertexPropertiesPanel).getCurrentInteractor();
			if(itr != nullptr && itr->is3DInteractor())
			{
				if(Interactor3D* i3d = dynamic_cast<Interactor3D*>(itr))
					i3d->extrudeVertices();
			}
		}

		else if(item == DebugLogging)
		{
			getSetting(DebugLogging) ^= true;
		}

		else if(item == SelectWithRight)
		{
			getSetting(SelectWithRight) ^= true;
		}

		else if(item == NativeDialogs)
		{
			getSetting(NativeDialogs) ^= true;
		}

		else if(item == SnapEdit)
		{
			getSetting(SnapMode) ^= true;
		}

		else if(item == CollisionDetection)
		{
			getSetting(CollisionDetection) ^= true;
		}

		else if(item == UpdateRealtime || item == UpdateOnRelease)
		{
			getSetting(UpdateGfxRealtime) = item == UpdateRealtime;
		}

		else if(item >= PitchAlgorithmAuto && item <= PitchAlgorithmSwipe)
		{
			switch(item)
			{
				case PitchAlgorithmAuto: 	getSetting(PitchAlgo) = PitchAlgos::AlgoAuto;
				case PitchAlgorithmYin: 	getSetting(PitchAlgo) = PitchAlgos::AlgoYin;
				case PitchAlgorithmSwipe: 	getSetting(PitchAlgo) = PitchAlgos::AlgoSwipe;
			}
		}

		else if(item == PitchOctaveUp || item == PitchOctaveDown)
		{
			getObj(SampleUtils).shiftWaveNoteOctave(item == PitchOctaveUp);
		}

		else if(item >= NoteStart)
		{
			int midiNote = item - NoteStart;

			getObj(SampleUtils).updateMidiNoteNumber(midiNote);
		}
	}
	else if(topLevelMenuIndex == GraphicsMenu)
	{
		if (item == VertsOnHover)
		{
			getSetting(ViewVertsOnlyOnHover) ^= true;
			getObj(Updater).update(UpdateSources::SourceAll, (int) UpdateType::Repaint);
		}

		else if(item >= ViewStageA && item <= ViewStageD)
		{
			int newViewStage = ViewStages::PreProcessing;

			if(item == ViewStageA)		newViewStage = ViewStages::PreProcessing;
			else if(item == ViewStageB)	newViewStage = ViewStages::PostEnvelopes;
			else if(item == ViewStageC)	newViewStage = ViewStages::PostSpectrum;
			else if(item == ViewStageD)	newViewStage = ViewStages::PostFX;

			if (Util::assignAndWereDifferent(getSetting(ViewStage), newViewStage))
			{
				dout << "view stage set to " << String((int) getSetting(ViewStage)) << "\n";

				// XXX
//				getObj(Updater).viewStageChanged();
				doUpdate(SourceMorph);
			}
		}

		else if(item == WaveformWaterfall)
		{
			getSetting(Waterfall) ^= true;
			getObj(Waveform2D).repaint();
		}

		else if(item == DrawScales)
		{
			getSetting(DrawScales) ^= true;
		}

		/*
		else if(item == UseOpenGL)
		{
//			getSetting(UseOpenGL) ^= true;
			getObj(MainPanel).switchedRenderingMode(true);
		}
		*/

		else if(item >= PointSize1x && item <= PointSize4x)
		{
			int& size = getSetting(PointSizeScale);
			int newSize = ScaleSizes::ScaleSmall;

			switch(item)
			{
				case PointSize1x: newSize = ScaleSizes::ScaleSmall; break;
				case PointSize2x: newSize = ScaleSizes::ScaleMed; break;
				case PointSize4x: newSize = ScaleSizes::ScaleLarge; break;
			}

			if(Util::assignAndWereDifferent(size, newSize))
			{
				getObj(Updater).update(UpdateSources::SourceAll, (int) UpdateType::Repaint);
			}
		}

		else if(item >= Reduction1x && item <= Reduction5x)
		{
			int& factor = getSetting(ReductionFactor);
			int newFactor = 5;
			switch(item)
			{
				case Reduction1x: newFactor = 1; break;
				case Reduction3x: newFactor = 3; break;
				case Reduction5x: newFactor = 5; break;
			}

			// XXX
			/*
			if(Util::assignAndWereDifferent(factor, newFactor))
				getObj(Updater).reductionFactorChanged();
			*/
		}

		else if(item == InterpWaveCycles || item == WrapWaveCycles)
		{
			if(item == InterpWaveCycles)
				getSetting(InterpWaveCycles) ^= true;
			else
				getSetting(WrapWaveCycles) ^= true;

			if(getSetting(DrawWave))
				doUpdate(SourceMorph);
		}
	}

	else if (topLevelMenuIndex == AudioMenu)
	{
		if(item == QualityOptions)
		{
			getObj(Dialogs).showQualityOptions();
		}
		else if(item >= OversampRltm1x && item <= OversampRltm16x)
		{
			int id = (item - OversampRltm1x) + QualityDialog::OversampRltm1x;

			getObj(QualityDialog).triggerOversample(id);
		}
		else if(item == ModMatrix)
		{
			getObj(Dialogs).showModMatrix();
		}
		else if(item == Declick)
		{
			getDocSetting(Declick) ^= true;
			editedSomething = true;
		}
		else if(item >= BendRange2 && item <= BendRange12)
		{
			int range = 2;
			switch(item)
			{
				case BendRange2: 	range = 2; 	break;
				case BendRange6: 	range = 6; 	break;
				case BendRange12:  	range = 12; break;
			}

			if(Util::assignAndWereDifferent(getDocSetting(PitchBendRange), range))
				editedSomething = true;
		}
		else if(item == DynamicEnvs)
		{
			getDocSetting(DynamicEnvelopes) ^= true;
			editedSomething = true;
		}
	}

	else if(topLevelMenuIndex == HelpMenu)
	{
		if(item == About)			getObj(Dialogs).showAboutDialog();
//		else if(item == Help)		getObj(Dialogs).launchHelp();
		else if(item == Tour)		getObj(CycleTour).enter();
//		else if(item == Cheatsheet)	getObj(Dialogs).launchCheatsheetDocument();
		else if(item >= Tutorials)
		{
			int fileIndex = item - Tutorials;

			jassert(fileIndex < tutorials.size());

			File& file = tutorials.getReference(fileIndex);

			XmlDocument doc(file.loadFileAsString());
			ScopedPointer<XmlElement> top = doc.getDocumentElement();

			if(doc.getLastParseError().isEmpty())
			{
				getObj(CycleTour).readXML(top);
			}
			else
			{
				showCritical("Xml parse error: " + doc.getLastParseError());
			}
		}
	}

	if(editedSomething)
		getObj(EditWatcher).setHaveEditedWithoutUndo(true);
}


void SynthMenuBarModel::init()
{
	miscGfx = &getObj(MiscGraphics);
	source 	= &getObj(SynthAudioSource);

	File tutorialDir(getObj(Directories).getTutorialDir());
	tutorialDir.findChildFiles(tutorials, File::findFiles, false, "*.xml");
}


void SynthMenuBarModel::triggerClick(int stage)
{
	switch(stage)
	{
		case ViewStageA: menuItemSelected(ViewStageA, GraphicsMenu); break;
		case ViewStageB: menuItemSelected(ViewStageB, GraphicsMenu); break;
		case ViewStageC: menuItemSelected(ViewStageC, GraphicsMenu); break;
		case ViewStageD: menuItemSelected(ViewStageD, GraphicsMenu); break;
	}
}


EditableItem::EditableItem(const String& name) : name(name)
{
}


void EditableItem::resized()
{
}


void EditableItem::buttonClicked(Button* button)
{
}

void EditableItem::getIdealSize(int& idealWidth, int& idealHeight)
{
}

