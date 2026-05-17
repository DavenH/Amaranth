#include <App/Doc/Document.h>
#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Curve/Mesh/Mesh.h>
#include <Design/Updating/Updater.h>
#include <Inter/UndoableMeshProcess.h>
#include <Inter/Interactor.h>
#include <Inter/Interactor2D.h>
#include <UI/Panels/ZoomPanel.h>

#include "Dialogs.h"
#include "KeyboardInputHandler.h"

#include "../App/CycleTour.h"
#include "../Audio/AudioSourceRepo.h"
#include "../Audio/SampleUtils.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../Inter/EnvelopeInter3D.h"
#include "../Inter/SpectrumInter2D.h"
#include "../Inter/SpectrumInter3D.h"
#include "../Inter/WaveformInter2D.h"
#include "../Inter/WaveformInter3D.h"
#include "../UI/Dialogs/PresetPage.h"
#include "../UI/Panels/Console.h"
#include "../UI/Panels/GeneralControls.h"
#include "../UI/Panels/MainPanel.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/Panels/SynthMenuBarModel.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform2D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "FileManager.h"

KeyboardInputHandler::KeyboardInputHandler(SingletonRepo* repo) :
        SingletonAccessor(repo, "KeyboardInputHandler") {
}

void KeyboardInputHandler::init() {
    position = &getObj(PlaybackPanel);
    waveInter3D 	= &getObj(WaveformInter3D);
    main			= &getObj(MainPanel);
    console 		= &getObj(Console);

    currentInteractor 	= waveInter3D;

    commandManager.setFirstCommandTarget(this);
    commandManager.registerAllCommandsForTarget(this);
    commandManager.getKeyMappings()->resetToDefaultMappings();
}

bool KeyboardInputHandler::keyPressed(const KeyPress &key, juce::Component* component) {
    return commandManager.getKeyMappings()->keyPressed(key, component);
}

ApplicationCommandManager& KeyboardInputHandler::getCommandManager() {
    return commandManager;
}

ApplicationCommandTarget* KeyboardInputHandler::getNextCommandTarget() {
    return nullptr;
}

void KeyboardInputHandler::getAllCommands(Array<CommandID>& commands) {
    const CommandID ids[] = {
        CommandNewFile,
        CommandOpenFile,
        CommandSaveFile,
        CommandSaveAsFile,
        CommandUndo,
        CommandRedo,
        CommandEraseSelected,
        CommandExtrudeSelected,
        CommandTogglePlayback,
        CommandTransportToStart,
        CommandTransportToEnd,
        CommandRedToStart,
        CommandRedToEnd,
        CommandBlueToStart,
        CommandBlueToEnd,
        CommandNavigateMorph,
        CommandNextPreset,
        CommandPreviousPreset,
        CommandShowFilterTab,
        CommandShowResynthesisTab,
        CommandTourPrevious,
        CommandTourNext,
        CommandSetAdditiveMode,
        CommandSetFilterMode,
        CommandShowPresetBrowser,
        CommandDeselectAll,
        CommandConnectSelected,
        CommandToggleYellowLink,
        CommandToggleRedLink,
        CommandToggleBlueLink,
        CommandSelectTool,
        CommandPencilTool,
        CommandAxeTool,
        CommandTimeDimension,
        CommandRedDimension,
        CommandBlueDimension,
        CommandToggleMagnitudeDraw,
        CommandToggleWaveOverlay,
        CommandEscape,
        CommandCycleTool,
        CommandDebugDumpPreset,
      #ifdef _DEBUG
        CommandDebugPrintMesh,
      #endif
    };

    commands.addArray(ids, numElementsInArray(ids));
}

void KeyboardInputHandler::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) {
    const ModifierKeys command(ModifierKeys::commandModifier);
    const ModifierKeys shift(ModifierKeys::shiftModifier);
    const ModifierKeys commandShift(ModifierKeys::commandModifier | ModifierKeys::shiftModifier);

    switch (commandID) {
        case CommandNewFile:
            result.setInfo("New", "Create a new preset.", "File", 0);
            result.addDefaultKeypress('n', command);
            break;
        case CommandOpenFile:
            result.setInfo("Open", "Open a preset.", "File", 0);
            result.addDefaultKeypress('o', command);
            break;
        case CommandSaveFile:
            result.setInfo("Save", "Save the current preset.", "File", 0);
            result.addDefaultKeypress('s', command);
            result.setActive(getObj(EditWatcher).getHaveEdited());
            break;
        case CommandSaveAsFile:
            result.setInfo("Save as...", "Save the current preset under a new name.", "File", 0);
            result.addDefaultKeypress('s', commandShift);
            break;
        case CommandUndo:
            result.setInfo("Undo", "Undo the previous edit.", "Edit", 0);
            result.addDefaultKeypress('z', command);
            result.setActive(getObj(EditWatcher).getUndoManager().canUndo());
            break;
        case CommandRedo:
            result.setInfo("Redo", "Redo the previous edit.", "Edit", 0);
            result.addDefaultKeypress('z', commandShift);
            result.setActive(getObj(EditWatcher).getUndoManager().canRedo());
            break;
        case CommandEraseSelected:
            result.setInfo("Erase selected", "Erase selected vertices.", "Edit", 0);
            result.addDefaultKeypress(KeyPress::deleteKey, ModifierKeys());
            result.addDefaultKeypress(KeyPress::backspaceKey, ModifierKeys());
            result.setActive(currentInteractor != nullptr && ! currentInteractor->getSelected().empty());
            break;
        case CommandExtrudeSelected:
            result.setInfo("Extrude selected", "Extrude selected vertices.", "Edit", 0);
            result.addDefaultKeypress('e', ModifierKeys());
            result.setActive(currentInteractor != nullptr && currentInteractor->is3DInteractor());
            break;
        case CommandTogglePlayback:
            result.setInfo("Toggle playback", "Start or stop playback.", "Transport", 0);
            result.addDefaultKeypress(' ', ModifierKeys());
            result.addDefaultKeypress(KeyPress::returnKey, ModifierKeys());
            break;
        case CommandTransportToStart:
            result.setInfo("Start", "Move transport to start.", "Transport", 0);
            result.addDefaultKeypress(KeyPress::homeKey, ModifierKeys());
            break;
        case CommandTransportToEnd:
            result.setInfo("End", "Move transport to end.", "Transport", 0);
            result.addDefaultKeypress(KeyPress::endKey, ModifierKeys());
            break;
        case CommandRedToStart:
            result.setInfo("Red to start", "Move red morph to start.", "Morph", 0);
            result.addDefaultKeypress(KeyPress::homeKey, shift);
            break;
        case CommandRedToEnd:
            result.setInfo("Red to end", "Move red morph to end.", "Morph", 0);
            result.addDefaultKeypress(KeyPress::endKey, shift);
            break;
        case CommandBlueToStart:
            result.setInfo("Blue to start", "Move blue morph to start.", "Morph", 0);
            result.addDefaultKeypress(KeyPress::homeKey, command);
            break;
        case CommandBlueToEnd:
            result.setInfo("Blue to end", "Move blue morph to end.", "Morph", 0);
            result.addDefaultKeypress(KeyPress::endKey, command);
            break;
        case CommandNavigateMorph:
            result.setInfo("Navigate morph", "Move the current morph position.", "Morph", 0);
            for (auto modifiers : { ModifierKeys(), shift, command }) {
                result.addDefaultKeypress(KeyPress::pageUpKey, modifiers);
                result.addDefaultKeypress(KeyPress::pageDownKey, modifiers);
                result.addDefaultKeypress(KeyPress::leftKey, modifiers);
                result.addDefaultKeypress(KeyPress::rightKey, modifiers);
            }
            break;
        case CommandNextPreset:
            result.setInfo("Next preset", "Select the next preset.", "Navigation", 0);
            result.addDefaultKeypress(KeyPress::fastForwardKey, ModifierKeys());
            break;
        case CommandPreviousPreset:
            result.setInfo("Previous preset", "Select the previous preset.", "Navigation", 0);
            result.addDefaultKeypress(KeyPress::rewindKey, ModifierKeys());
            break;
        case CommandShowFilterTab:
            result.setInfo("Filter tab", "Show the filter tab.", "View", 0);
            result.addDefaultKeypress('f', command);
            break;
        case CommandShowResynthesisTab:
            result.setInfo("Resynthesis tab", "Show the resynthesis tab.", "View", 0);
            result.addDefaultKeypress('r', command);
            break;
        case CommandTourPrevious:
            result.setInfo("Previous tour item", "Show the previous tour item.", "Tour", 0);
            result.addDefaultKeypress('[', ModifierKeys());
            break;
        case CommandTourNext:
            result.setInfo("Next tour item", "Show the next tour item.", "Tour", 0);
            result.addDefaultKeypress(']', ModifierKeys());
            break;
        case CommandSetAdditiveMode:
            result.setInfo("Additive mode", "Set additive spectrum mode.", "Spectrum", 0);
            result.addDefaultKeypress('+', ModifierKeys());
            break;
        case CommandSetFilterMode:
            result.setInfo("Filter mode", "Set filter spectrum mode.", "Spectrum", 0);
            result.addDefaultKeypress('-', ModifierKeys());
            break;
        case CommandShowPresetBrowser:
            result.setInfo("Preset browser", "Show the preset browser.", "File", 0);
            result.addDefaultKeypress('p', ModifierKeys());
            break;
        case CommandDeselectAll:
            result.setInfo("Deselect all", "Clear the current selection.", "Edit", 0);
            result.addDefaultKeypress('a', ModifierKeys());
            result.setActive(currentInteractor != nullptr);
            break;
        case CommandConnectSelected:
            result.setInfo("Connect selected", "Connect selected vertices.", "Edit", 0);
            result.addDefaultKeypress('c', ModifierKeys());
            result.setActive(currentInteractor != nullptr && currentInteractor->is3DInteractor());
            break;
        case CommandToggleYellowLink:
            result.setInfo("Toggle yellow link", "Toggle yellow vertex linking.", "Edit", 0);
            result.addDefaultKeypress('t', ModifierKeys());
            result.addDefaultKeypress('7', ModifierKeys());
            break;
        case CommandToggleRedLink:
            result.setInfo("Toggle red link", "Toggle red vertex linking.", "Edit", 0);
            result.addDefaultKeypress('k', ModifierKeys());
            result.addDefaultKeypress('8', ModifierKeys());
            break;
        case CommandToggleBlueLink:
            result.setInfo("Toggle blue link", "Toggle blue vertex linking.", "Edit", 0);
            result.addDefaultKeypress('m', ModifierKeys());
            result.addDefaultKeypress('9', ModifierKeys());
            break;
        case CommandSelectTool:
            result.setInfo("Select tool", "Use the selection tool.", "Tools", 0);
            result.addDefaultKeypress('4', ModifierKeys());
            break;
        case CommandPencilTool:
            result.setInfo("Pencil tool", "Use the pencil tool.", "Tools", 0);
            result.addDefaultKeypress('5', ModifierKeys());
            break;
        case CommandAxeTool:
            result.setInfo("Axe tool", "Use the axe tool.", "Tools", 0);
            result.addDefaultKeypress('6', ModifierKeys());
            break;
        case CommandTimeDimension:
            result.setInfo("Time dimension", "Use the time dimension.", "Morph", 0);
            result.addDefaultKeypress('1', ModifierKeys());
            break;
        case CommandRedDimension:
            result.setInfo("Red dimension", "Use the red dimension.", "Morph", 0);
            result.addDefaultKeypress('2', ModifierKeys());
            break;
        case CommandBlueDimension:
            result.setInfo("Blue dimension", "Use the blue dimension.", "Morph", 0);
            result.addDefaultKeypress('3', ModifierKeys());
            break;
        case CommandToggleMagnitudeDraw:
            result.setInfo("Toggle magnitude draw", "Toggle magnitude drawing mode.", "Graphics", 0);
            result.addDefaultKeypress('h', ModifierKeys());
            result.addDefaultKeypress('/', ModifierKeys());
            break;
        case CommandToggleWaveOverlay:
            result.setInfo("Toggle wave overlay", "Show or hide the wave overlay.", "Graphics", 0);
            result.addDefaultKeypress('w', ModifierKeys());
            result.addDefaultKeypress('\\', ModifierKeys());
            break;
        case CommandEscape:
            result.setInfo("Cancel", "Cancel the current interaction.", "Navigation", 0);
            result.addDefaultKeypress(KeyPress::escapeKey, ModifierKeys());
            break;
        case CommandCycleTool:
            result.setInfo("Cycle tool", "Cycle through tools.", "Tools", 0);
            result.addDefaultKeypress('l', ModifierKeys());
            break;
        case CommandDebugDumpPreset:
            result.setInfo("Debug dump preset", "Log the current preset XML.", "Debug", 0);
            result.addDefaultKeypress('q', ModifierKeys());
            break;
      #ifdef _DEBUG
        case CommandDebugPrintMesh:
            result.setInfo("Debug print mesh", "Log the current mesh.", "Debug", 0);
            result.addDefaultKeypress('v', ModifierKeys());
            break;
      #endif
        default:
            break;
    }
}

bool KeyboardInputHandler::perform(const InvocationInfo& info) {
    return performCommand(info.commandID, info.keyPress, info.originatingComponent);
}

bool KeyboardInputHandler::performCommand(CommandID commandID, const KeyPress& key, Component* component) {
    ignoreUnused(component);

    bool editedSomething 	= false;
    bool updateBlue			= false;

    auto* itr3D 	 = dynamic_cast<Interactor3D*>(currentInteractor);
    auto& morphPanel = getObj(MorphPanel);

    if(currentInteractor) {
        currentInteractor->flag(DidMeshChange) = false;
        currentInteractor->flag(SimpleRepaint) = false;
    }

    info("command: " << String(int(commandID)) << "\n");

    if (commandID == CommandNewFile) {
        getObj(SynthMenuBarModel).menuItemSelected(SynthMenuBarModel::NewFile, SynthMenuBarModel::FileMenu);
    } else if (commandID == CommandOpenFile) {
        getObj(SynthMenuBarModel).menuItemSelected(SynthMenuBarModel::OpenFile, SynthMenuBarModel::FileMenu);
    } else if (commandID == CommandSaveFile) {
        getObj(SynthMenuBarModel).menuItemSelected(SynthMenuBarModel::SaveFile, SynthMenuBarModel::FileMenu);
    } else if (commandID == CommandSaveAsFile) {
        getObj(SynthMenuBarModel).menuItemSelected(SynthMenuBarModel::SaveAsFile, SynthMenuBarModel::FileMenu);
    } else if (commandID == CommandUndo) {
        getObj(SynthMenuBarModel).menuItemSelected(SynthMenuBarModel::UndoEdit, SynthMenuBarModel::EditMenu);
    } else if (commandID == CommandRedo) {
        getObj(SynthMenuBarModel).menuItemSelected(SynthMenuBarModel::RedoEdit, SynthMenuBarModel::EditMenu);
    } else if (commandID == CommandTogglePlayback) {
        position->togglePlayback();
    } else if (commandID == CommandTransportToEnd) {
        position->setProgress(1.0f);
    } else if (commandID == CommandBlueToEnd) {
        morphPanel.triggerValue(Vertex::Blue, 1.f);
    } else if (commandID == CommandRedToEnd) {
        morphPanel.triggerValue(Vertex::Red, 1.f);
    } else if (commandID == CommandTransportToStart) {
        position->resetPlayback(true);
    } else if (commandID == CommandBlueToStart) {
        morphPanel.triggerValue(Vertex::Blue, 0.f);
    } else if (commandID == CommandRedToStart) {
        morphPanel.triggerValue(Vertex::Red, 0.f);
    } else if (commandID == CommandNavigateMorph) {
        int code = key.getKeyCode();
        bool cmdDown = key.getModifiers().isCommandDown();
        bool shftDown = key.getModifiers().isShiftDown();

        if (getObj(CycleTour).isLive() && (code == KeyPress::leftKey || code == KeyPress::rightKey)) {
            code == KeyPress::leftKey ?
                    getObj(CycleTour).showPrevious() :
                    getObj(CycleTour).showNext();
        } else {
            int dim = cmdDown ? Vertex::Blue : shftDown ? Vertex::Red : Vertex::Time;

            float value 		= getObj(MorphPanel).getValue(dim);
            float windowSize 	= getObj(Waveform3D).getZoomPanel()->rect.w;
            float increment  	= (code == KeyPress::pageUpKey || code == KeyPress::pageDownKey) ? 0.15f * windowSize : 0.005f * windowSize;
            float direction  	= (code == KeyPress::pageUpKey || code == KeyPress::leftKey) 	 ? -1.f : 1.f;
            float newPosition 	= value + direction * increment;

            NumberUtils::constrain(newPosition, 0.f, 1.f);

            getObj(MorphPanel).triggerValue(dim, newPosition);

            if(dim == getSetting(CurrentMorphAxis)) {
                position->setProgress(newPosition);
            }
        }
    } else if (commandID == CommandNextPreset) {
        getObj(PresetPage).triggerButtonClick(PresetPage::NextButton);
    } else if (commandID == CommandPreviousPreset) {
        getObj(PresetPage).triggerButtonClick(PresetPage::PrevButton);
    } else if (commandID == CommandShowFilterTab) {
        main->triggerTabClick(1);
    } else if (commandID == CommandShowResynthesisTab) {
        main->triggerTabClick(0);
    } else if (commandID == CommandTourPrevious || commandID == CommandTourNext) {
        if (getObj(CycleTour).isLive()) {
            commandID == CommandTourPrevious ? getObj(CycleTour).showPrevious() :
                                               getObj(CycleTour).showNext();
        }
    } else if (commandID == CommandSetAdditiveMode || commandID == CommandSetFilterMode) {
        getObj(Spectrum3D).triggerButton(commandID == CommandSetAdditiveMode ? CycleTour::IdBttnModeAdditive
                                                                              : CycleTour::IdBttnModeFilter);
    } else if (commandID == CommandShowPresetBrowser) {
        getObj(Dialogs).showPresetBrowserModal();
    } else if (commandID == CommandDeselectAll) {
        currentInteractor->deselectAll();
    } else if (commandID == CommandConnectSelected) {
        if (itr3D) {
            editedSomething = itr3D->connectSelected();
        }
    } else if (commandID == CommandEraseSelected) {
        if (currentInteractor == nullptr) {
            return false;
        }

        currentInteractor->eraseSelected();

        editedSomething = true;
    } else if (commandID == CommandExtrudeSelected) {
        if (itr3D) {
            itr3D->extrudeVertices();
            editedSomething = true;
        }
    } else if (commandID == CommandToggleYellowLink) {
        getSetting(LinkYellow) ^= true;
        updateBlue = true;
    } else if (commandID == CommandToggleRedLink) {
        getSetting(LinkRed) ^= true;
        updateBlue = true;
    } else if (commandID == CommandToggleBlueLink) {
        getSetting(LinkBlue) ^= true;
        updateBlue = true;
    } else if (commandID == CommandSelectTool || commandID == CommandPencilTool || commandID == CommandAxeTool) {
        getSetting(Tool) = commandID == CommandSelectTool ? Tools::Selector :
                           commandID == CommandPencilTool ? Tools::Pencil :
                                                            Tools::Axe;
        getObj(GeneralControls).updateHighlights();
    } else if (commandID == CommandTimeDimension || commandID == CommandRedDimension || commandID == CommandBlueDimension) {
        main->setPrimaryDimension(commandID == CommandTimeDimension ? Vertex::Time :
                                  commandID == CommandRedDimension  ? Vertex::Red  :
                                                                       Vertex::Blue, true);
        getObj(MorphPanel).updateHighlights();
    } else if(commandID == CommandDebugDumpPreset)	{
      #ifdef JUCE_DEBUG
        String detailsString = getObj(Document).getPresetString();
        info(detailsString << "\n");
      #endif
    } else if (commandID == CommandToggleMagnitudeDraw) {
        getSetting(MagnitudeDrawMode) ^= 1;

        Spectrum3D* f = &getObj(Spectrum3D);

        f->modeChanged(getSetting(MagnitudeDrawMode) == 1, true);
        f->updateKnobValue();
        f->setIconHighlightImplicit();
    } else if (commandID == CommandToggleWaveOverlay) {
        if (getSetting(WaveLoaded)) {
            bool isWave = !getSetting(DrawWave);
            getObj(SampleUtils).waveOverlayChanged(isWave);
        } else {
            showConsoleMsg("Load a wave file first!");
        }
    } else if (commandID == CommandEscape) {
        if (getObj(CycleTour).isLive()) {
            getObj(CycleTour).exit();
        } else {
            currentInteractor->deselectAll(true);
        }
    }

  #ifdef _DEBUG
    else if (commandID == CommandDebugPrintMesh) {
        currentInteractor->getMesh()->print(true, false);
    }
  #endif

    else if (commandID == CommandCycleTool) {
        int &tool = getSetting(Tool);
        tool++;

        if (tool > Tools::Axe) {
            tool = Tools::Selector;
        }
    } else {
        return false;
    }

    finishSelectionEdit(editedSomething, updateBlue);

    return true;
}

void KeyboardInputHandler::finishSelectionEdit(bool editedSomething, bool updateBlue) {
    if (editedSomething) {
        getObj(EditWatcher).setHaveEditedWithoutUndo(true);
    }

    if (updateBlue) {
        updateLinkedVertexHighlights();
    }

    commandManager.commandStatusChanged();
}

void KeyboardInputHandler::updateLinkedVertexHighlights() {
    getObj(MorphPanel).updateHighlights();

    getObj(WaveformInter3D).setMovingVertsFromSelected();
    getObj(WaveformInter2D).setMovingVertsFromSelected();
    getObj(SpectrumInter2D).setMovingVertsFromSelected();
    getObj(SpectrumInter3D).setMovingVertsFromSelected();
    getObj(EnvelopeInter2D).setMovingVertsFromSelected();
    getObj(EnvelopeInter3D).setMovingVertsFromSelected();
}

void KeyboardInputHandler::setFocusedInteractor(Interactor* interactor, bool isMeshInteractor) {
    currentInteractor = interactor;
    this->isMeshInteractor = isMeshInteractor;
    commandManager.commandStatusChanged();
}

Interactor* KeyboardInputHandler::getCurrentInteractor() {
    return currentInteractor;
}
