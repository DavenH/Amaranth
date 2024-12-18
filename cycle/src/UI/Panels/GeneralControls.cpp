#include <App/Doc/Document.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Inter/Interactor.h>
#include <UI/IConsole.h>
#include <UI/MiscGraphics.h>
#include <UI/Widgets/CalloutUtils.h>

#include "GeneralControls.h"
#include "VertexPropertiesPanel.h"
#include "PlaybackPanel.h"

#include "../CycleDefs.h"
#include "../Dialogs/PresetPage.h"
#include "../Panels/Morphing/MorphPanel.h"
#include "../VertexPanels/Spectrum2D.h"
#include "../VertexPanels/Spectrum3D.h"

#include "../../App/CycleTour.h"
#include "../../App/Dialogs.h"
#include "../../App/FileManager.h"
#include "../../UI/VisualDsp.h"
#include "../../Audio/AudioSourceRepo.h"
#include "../../Audio/SampleUtils.h"
#include "../../Inter/SpectrumInter3D.h"
#include "../CycleGraphicsUtils.h"


GeneralControls::GeneralControls(SingletonRepo* repo) : 
        SingletonAccessor(repo, "GeneralControls")

    ,	linkYellow(	1, 1, this, repo, "Move the ends of lines in unison", "t, 7")

    ,	waveVerts(	0, 5, this, repo, "Show sample with verts overlay", "w")
    ,	verts(		2, 5, this, repo, "Show verts only", "w")

    ,	play(		0, 0, 3, 0, this, repo, "Play", 	"space", "Pause", "space")
    ,	rewind(		1, 0, this, repo, "Rewind", 		"home")
    ,	ffwd(		2, 0, this, repo, "To end", 		"end")

    ,	pointer(	0, 6, this, repo, "Default selector", "4")
    ,	pencil(		1, 6, this, repo, "Pencil", 		 "5")
    ,	axe(		2, 6, this, repo, "Axe", 		     "6")
    ,	nudge(		9, 0, this, repo, "Phase Nudge", 	 "")

    , 	prev(		2, 8, this, repo, "Previous Preset", "|<")
    , 	next(		3, 8, this, repo, "Next Preset", 	 ">|")
    ,	save(		2, 3, this, repo, "Save current preset")

//	,	snap(		3, 1, this, repo, "Snap")
    ,	tableIcon(	8, 5, this, repo, "Presets") {
}

void GeneralControls::init() {
    dialogs		 = &getObj(Dialogs);
    position 	 = &getObj(PlaybackPanel);
    audioManager = &getObj(AudioSourceRepo);
    fileManager	 = &getObj(FileManager);

    Component* waveTemp[] =  { &waveVerts, 	&verts 						};
    Component* transTemp[] = { &play, 		&rewind, 	&ffwd 			};
    Component* prstTemp[] =  { &save, 		&tableIcon, &prev, 	&next	};
    Component* toolTemp[] =  { &pointer, 	&pencil, 	&axe, 	&nudge	};

    CalloutUtils::addRetractableCallout(waveCO,  wavePO,  repo, 0, 5, waveTemp,  numElementsInArray(waveTemp), 	this, true);
    CalloutUtils::addRetractableCallout(transCO, transPO, repo, 0, 0, transTemp, numElementsInArray(transTemp), this, true);
    CalloutUtils::addRetractableCallout(prstCO,  prstPO,  repo, 2, 3, prstTemp,  numElementsInArray(prstTemp), 	this, true);
    CalloutUtils::addRetractableCallout(toolCO,  toolPO,  repo, 1, 7, toolTemp,  numElementsInArray(toolTemp), 	this, true);

    topCallouts.push_back(toolCO.get());
    topCallouts.push_back(prstCO.get());
    topCallouts.push_back(transCO.get());
    topCallouts.push_back(waveCO.get());

  #ifdef DEMO_VERSION
    save.setApplicable(false);
    save.setNotApplicableMessage("Saving is disabled in this demo.");
  #endif

    waveVerts.setNotApplicableMessage("Load a wave file first to see the sample view");

    updateHighlights();
}


void GeneralControls::buttonClicked(Button* button) {
    if (button == &waveVerts || button == &verts) {
        if (button != &verts && !getSetting(WaveLoaded)) {
            showImportant("Load a wave file first!");
        } else {
            getObj(PlaybackPanel).stopPlayback();
            bool isWave = (button == &waveVerts);

            getObj(SampleUtils).waveOverlayChanged(isWave);
        }
    } else if (button == &prev) {
        getObj(PresetPage).triggerButtonClick(PresetPage::PrevButton);
    } else if (button == &next) {
        getObj(PresetPage).triggerButtonClick(PresetPage::NextButton);
    } else if (button == &save) {
        fileManager->saveCurrentPreset();
    } else if (button == &linkYellow) {
        getObj(MorphPanel).triggerClick(CycleTour::IdBttnLinkY);
    } else if (button == &tableIcon) {
        getObj(Dialogs).showPresetBrowserModal();
    } else if (button == &play) {
        position->togglePlayback();
    }

    else if (button == &rewind)		position->resetPlayback(true);
    else if (button == &ffwd)		position->setProgress(1.0f);
    else if (button == &pointer)	getSetting(Tool) = Tools::Selector;
    else if (button == &pencil)		getSetting(Tool) = Tools::Pencil;
    else if (button == &axe)		getSetting(Tool) = Tools::Axe;
    else if (button == &nudge)		getSetting(Tool) = Tools::Nudge;

    updateHighlights();
    repaint();
}

void GeneralControls::paint(Graphics& g) {
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

    auto& mg = getObj(MiscGraphics);
    g.setColour(Colour(60, 60, 60));

    mg.drawJustifiedText(g, "TOOL", 		pointer, 	axe, 	true, 	toolCO.get());
    mg.drawJustifiedText(g, "PRESET", 		save, 		next, 		true, 	prstCO.get());
    mg.drawJustifiedText(g, "TRANSPORT", 	play, 		ffwd, 		true, 	transCO.get());
    mg.drawJustifiedText(g, "SURF VIEW", 	waveVerts, 	verts, 		true, 	waveCO.get());
}

void GeneralControls::paintOverChildren(Graphics& g) {
    if (tableIcon.getPendingItems() > 0 && !prstCO->isCollapsed()) {
        Point<int> topLeft = prstCO->getBounds().getTopLeft();
        Rectangle<int> rect = tableIcon.getBounds().translated(topLeft.getX(), topLeft.getY() + 24);
        getObj(MiscGraphics).drawJustifiedText(g, String(tableIcon.getPendingItems()), rect, true, nullptr);
    }
}

void GeneralControls::resized() {
    int xPosition, expandedSize, candidateLeftPosition;

    const int padding = 10;
    const int calloutMinWidth = bSize + 2 * padding;

    int y = getHeight() / 2 - 10;

    // we remember the x positions of the first row to use in the second
    vector<int> xPositions;

    int rawWidth = 0;
    for(auto callout : topCallouts)	{
        rawWidth += callout->getExpandedSize();
    }

    float spacer = (getWidth() - rawWidth) / float(topCallouts.size() + 1);

    if(spacer < padding) {
        spacer = padding;
    }

    expandedSize = candidateLeftPosition = 0;
    xPosition = spacer;

    for (int j = 0; j < topCallouts.size(); ++j) {
        xPositions.push_back(xPosition);

        RetractableCallout* callout = topCallouts[j];

        expandedSize = callout->getExpandedSize();
        int sizeOfIcons = (topCallouts.size() - 1 - j) * calloutMinWidth;
        candidateLeftPosition = xPosition + expandedSize + sizeOfIcons;

        bool isSpaceForExpandedCallout = candidateLeftPosition < getWidth() && !callout->isAlwaysCollapsed();

        if (isSpaceForExpandedCallout) {
            callout->setBounds(xPositions[j], y, expandedSize + 3, bSize);
            xPosition += expandedSize + spacer;
        } else {
            callout->setBounds(xPositions[j], y, bSize, bSize);
            xPosition += calloutMinWidth;
        }
    }
}

void GeneralControls::setPlayStopped() {
    play.setState(TwoStateButton::First);
}

void GeneralControls::setPlayStarted() {
    play.setState(TwoStateButton::Second);
}

void GeneralControls::sliderValueChanged(Slider* slider) {
}

void GeneralControls::mouseEnter(const MouseEvent& e) {
    getObj(IConsole).reset();
}

void GeneralControls::setNumCommunityPresets(int num) {
    tableIcon.setPendingItems(num);

    repaint();
}

void GeneralControls::updateHighlights() {
    int newTool = getSetting(Tool);
    pointer	.setHighlit(newTool == Tools::Selector);
    pencil	.setHighlit(newTool == Tools::Pencil);
    axe		.setHighlit(newTool == Tools::Axe);
    nudge	.setHighlit(newTool == Tools::Nudge);

    bool showWave = getSetting(DrawWave) == 1;

    waveVerts.setHighlit(showWave);
    verts.setHighlit(! showWave);
    waveVerts.setApplicable(getSetting(WaveLoaded) == 1);
}

Component* GeneralControls::getComponent(int which) {
    switch (which) {
        case CycleTour::TargSelector: 	return &pointer;
        case CycleTour::TargPencil: 	return &pencil;
        case CycleTour::TargAxe:		return &axe;
        case CycleTour::TargNudge:		return &nudge;
        case CycleTour::TargWaveVerts:	return &waveVerts;
        case CycleTour::TargVerts:		return &verts;
        case CycleTour::TargLinkYellow:	return &linkYellow;
        default:
            break;
    }

    return nullptr;
}
