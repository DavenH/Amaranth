#include <App/Doc/Document.h>
#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Curve/MeshRasterizer.h>
#include <Curve/Mesh.h>
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
}

bool KeyboardInputHandler::keyPressed(const KeyPress &key, Component* component) {
    char c 					= key.getTextCharacter();
    int code 				= key.getKeyCode();
    bool cmdDown			= key.getModifiers().isCommandDown();
    bool shftDown			= key.getModifiers().isShiftDown();
    bool altDown			= key.getModifiers().isAltDown();
    bool editedSomething 	= false;
    bool updateBlue			= false;

    auto* itr3D 	 = dynamic_cast<Interactor3D*>(currentInteractor);
    auto& morphPanel = getObj(MorphPanel);

    if(currentInteractor) {
        currentInteractor->flag(DidMeshChange) = false;
        currentInteractor->flag(SimpleRepaint) = false;
    }

    info("key down: " << c << ", " << code << "\n");

    if (c == ' ' || code == KeyPress::returnKey) {
//#if PLUGIN_MODE
//		return false;
//#else
        position->togglePlayback();
//#endif
    } else if (code == KeyPress::endKey) {

        if(cmdDown) {
            morphPanel.triggerValue(Vertex::Blue, 1.f);
        } else if(shftDown) {
            morphPanel.triggerValue(Vertex::Red, 1.f);
        } else {
            position->setProgress(1.0f);
        }
    } else if (code == KeyPress::homeKey) {
        if(cmdDown) {
            morphPanel.triggerValue(Vertex::Blue, 0.f);
        } else if(shftDown) {
            morphPanel.triggerValue(Vertex::Red, 0.f);
        } else {
            position->resetPlayback(true);
        }
    } else if (code == KeyPress::pageUpKey
               || code == KeyPress::pageDownKey
               || code == KeyPress::leftKey
                  || code == KeyPress::rightKey) {
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
    } else if (code == KeyPress::fastForwardKey) {
        getObj(PresetPage).triggerButtonClick(PresetPage::NextButton);
    } else if (code == KeyPress::rewindKey) {
        getObj(PresetPage).triggerButtonClick(PresetPage::PrevButton);
    } else if (code == CtrlF && cmdDown) {
        main->triggerTabClick(1);
    } else if (code == CtrlR && cmdDown) {
        main->triggerTabClick(0);
    } else if (c == '[' || c == ']') {
        if (getObj(CycleTour).isLive()) {
            c == '[' ? getObj(CycleTour).showPrevious() :
                       getObj(CycleTour).showNext();
        }
    } else if (c == '+' || c == '-') {
        getObj(Spectrum3D).triggerButton(c == '+' ? CycleTour::IdBttnModeAdditive : CycleTour::IdBttnModeFilter);
    } else if (c == 'p') {
        getObj(Dialogs).showPresetBrowserModal();
    } else if (c == 'a') {
        currentInteractor->deselectAll();
    } else if (c == 'c') {
        if (itr3D) {
            editedSomething = itr3D->connectSelected();
        }
    } else if (code == KeyPress::deleteKey || code == KeyPress::backspaceKey) {
        if (currentInteractor == nullptr) {
            return false;
        }

        currentInteractor->eraseSelected();

        editedSomething = true;
    }

//
//	else if (c == 'e')
//	{
//		if (itr3D)
//			itr3D->toggleAction(Actions::Extrude);
//
//		editedSomething = true;
//	}

    else if (c == 't' || c == '7') {
        getSetting(LinkYellow) ^= true;
        updateBlue = true;
    } else if (c == 'm' || c == '9') {
        getSetting(LinkBlue) ^= true;
        updateBlue = true;
    } else if (c == 'k' || c == '8') {
        getSetting(LinkRed) ^= true;
        updateBlue = true;
    } else if (c == '4' || c == '5' || c == '6') {
        getSetting(Tool) = c == '4' ? Tools::Selector : c == '5' ? Tools::Pencil : Tools::Axe;
        getObj(GeneralControls).updateHighlights();
    } else if (c == '1' || c == '2' || c == '3') {
        main->setPrimaryDimension(c == '1' ? Vertex::Time : c == '2' ? Vertex::Red : Vertex::Blue, true);
        getObj(MorphPanel).updateHighlights();
    } else if (code == CtrlZ && cmdDown) {
        if (shftDown) {
            getObj(EditWatcher).redo();
        } else {
            getObj(EditWatcher).undo();
        }
    } else if (code == CtrlS && cmdDown) {
        if (shftDown) {
            getObj(Dialogs).showPresetSaveAsDialog();
        } else {
            getObj(FileManager).saveCurrentPreset();
        }
    } else if(c == 'q')	{
      #ifdef JUCE_DEBUG
        String detailsString = getObj(Document).getPresetString();
        info(detailsString << "\n");
      #endif
    } else if (c == 'h' || c == '/') {
        getSetting(MagnitudeDrawMode) ^= 1;

        Spectrum3D* f = &getObj(Spectrum3D);

        f->modeChanged(getSetting(MagnitudeDrawMode) == 1, true);
        f->updateKnobValue();
        f->setIconHighlightImplicit();
    } else if (c == 'w' || c == '\\') {
        if (getSetting(WaveLoaded)) {
            bool isWave = !getSetting(DrawWave);
            getObj(SampleUtils).waveOverlayChanged(isWave);
        } else {
            showConsoleMsg("Load a wave file first!");
        }
    } else if (code == KeyPress::escapeKey) {
        if (getObj(CycleTour).isLive()) {
            getObj(CycleTour).exit();
        } else {
            currentInteractor->deselectAll(true);
        }
    }

  #ifdef _DEBUG
    else if (c == 'v') {
        currentInteractor->getMesh()->print(true, false);
    }
  #endif

    else if (c == 'l') {
        int &tool = getSetting(Tool);
        tool++;

        if (tool > Tools::Axe) {
            tool = Tools::Selector;
        }
    } else if (code == CtrlN) {
        getObj(Dialogs).promptForSaveApplicably(Dialogs::LoadEmptyPreset);
    } else {
        return false;
    }

    if (updateBlue) {
        getObj(MorphPanel).updateHighlights();

        getObj(WaveformInter3D).setMovingVertsFromSelected();
        getObj(WaveformInter2D).setMovingVertsFromSelected();
        getObj(SpectrumInter2D).setMovingVertsFromSelected();
        getObj(SpectrumInter3D).setMovingVertsFromSelected();
        getObj(EnvelopeInter2D).setMovingVertsFromSelected();
        getObj(EnvelopeInter3D).setMovingVertsFromSelected();
    }

    return true;
}

void KeyboardInputHandler::setFocusedInteractor(Interactor* interactor, bool isMeshInteractor) {
    currentInteractor = interactor;
    this->isMeshInteractor = isMeshInteractor;
}

Interactor* KeyboardInputHandler::getCurrentInteractor() {
    return currentInteractor;
}
