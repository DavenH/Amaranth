#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <Curve/EnvelopeMesh.h>
#include <Curve/EnvRasterizer.h>
#include <Inter/Interactor.h>
#include <UI/Layout/DynamicSizeContainer.h>
#include <UI/Layout/IDynamicSizeComponent.h>
#include <UI/MiscGraphics.h>
#include <UI/MouseOverMessager.h>
#include <UI/Panels/Panel.h>
#include <UI/Panels/Panel3D.h>
#include <UI/Widgets/CalloutUtils.h>

#include "PanelControls.h"
#include "LayerSelectionClient.h"
#include "../../VertexPanels/Envelope2D.h"
#include "../../Panels/MainPanel.h"
#include "../../CycleGraphicsUtils.h"

PanelControls::PanelControls(
		SingletonRepo* 			repo
	,	Interactor* 			interactor
	,	Button::Listener* 		listener
	,	LayerSelectionClient* 	client
	,	const String& 			layerString) :
			SingletonAccessor(repo, "PanelControls")
		, 	layerClient		(client)
		, 	interactor		(interactor)
		, 	scratchLabel	(getObj(MiscGraphics), "s.ch")
		, 	addRemover		(listener, repo, layerString)
		, 	upDownMover		(listener, repo)
		, 	enableCurrent	(5, 5, listener, repo, "Enable " + layerString)
		,	drawLabel		(false)
		,	haveEnablement	(false)
		,	haveDomains		(false)
		,	meshSelector	(nullptr)
		,	layerSelector	(nullptr)
		, 	spacer1			(1)
		, 	spacer2			(2)
		, 	spacer4			(4)
		, 	spacer6			(6)
		, 	spacer8			(8) {
}

void PanelControls::paint(Graphics& g) {
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

	MiscGraphics& mg = getObj(MiscGraphics);
	g.setFont(*mg.getSilkscreen());
	g.setColour(Colour::greyLevel(0.64f));

    if (haveDomains) {
//		Rectangle<int> r = domainCO->getBoundsInParentDelegate();
        mg.drawJustifiedText(g, "domain", domainBounds);
    }

    if (drawLabel) {
		mg.drawJustifiedText(g, "layer", layerLblBounds);
//		getObj(MiscGraphics).drawCentredText(g, layerLblBounds, "layer");
	}

	Colour base = Colour::greyLevel(0.11f);

	Array<Rectangle<int> > rects;
	Array<IDynamicSizeComponent*>* arrs[] = { &leftItems, &rightItems };

    for (int a = 0; a < numElementsInArray(arrs); ++a) {
        for (int i = 0; i < arrs[a]->size(); ++i) {
            IDynamicSizeComponent* c = arrs[a]->getUnchecked(i);

            if (c->outline && !c->isCurrentlyCollapsed()) {
                Rectangle<int> r = c->getBoundsInParentDelegate().translated(0, c->translateUp1 ? -1 : 0);
				rects.add(r);
			}
		}
	}

    for (int i = 0; i < rects.size(); ++i) {
        Rectangle<int> rect = rects[i];

        Path strokePath;
        strokePath.addRoundedRectangle(rect.getX(), rect.getY() + 1, rect.getWidth() + 1, rect.getHeight() - 2, 2.f);
        strokePath.applyTransform(AffineTransform::translation(-0.5f, -0.5f));

        g.setColour(Colour::greyLevel(0.02f));
        g.strokePath(strokePath, PathStrokeType(1.f));
    }

    if (!icon.isNull()) {
        g.drawImageAt(icon, iconBounds.getX(), iconBounds.getY());
	}
}

void PanelControls::resized() {
    Rectangle<int> bounds = getLocalBounds().reduced(4, 4);

    if (haveDomains) {
        domainBounds = bounds.removeFromTop(8);
		bounds.removeFromTop(2);
		domainCO->setBounds(bounds.removeFromTop(24).reduced(2, 0).translated(3, 0));
		bounds.removeFromTop(14);
	}

    if (drawLabel) {
        layerLblBounds = bounds.removeFromTop(11);
	}

	int leftHeight = 0, rightHeight = 0;

	Rectangle<int> layerBounds 	= bounds;
	Rectangle<int> leftSide 	= layerBounds.removeFromLeft(24);
	Rectangle<int> rightSide 	= layerBounds.removeFromRight(24);

    for (int i = 0; i < leftItems.size(); ++i) {
        IDynamicSizeComponent* item = leftItems[i];
		Rectangle<int> r(leftSide.removeFromTop(item->getExpandedSize()));

		if(item->getDynWidth() > 24)
			r.expand((item->getDynWidth() - 24) / 2, 0);

		item->setBoundsDelegate(r.getX(), r.getY(), r.getWidth(), r.getHeight());
	}

    for (int i = 0; i < rightItems.size(); ++i) {
        IDynamicSizeComponent* item = rightItems[i];
		Rectangle<int> r(rightSide.removeFromTop(item->getExpandedSize()));

		if(item->getDynWidth() > 24)
			r.expand((item->getDynWidth() - 24) / 2, 0);

		item->setBoundsDelegate(r.getX(), r.getY(), r.getWidth(), r.getHeight());
	}

	leftHeight = layerBounds.getHeight() - leftSide.getHeight();
	rightHeight = layerBounds.getHeight() - rightSide.getHeight();

	bounds.removeFromTop(jmax(leftHeight, rightHeight));
	bounds.removeFromTop(6);

	for (int i = 0; i < sliders.size(); ++i) {
        HSlider* slider = sliders[i];

		slider->setBounds(bounds.removeFromTop(13));
		bounds.removeFromTop(4);
	}

	bounds.removeFromTop(bounds.getHeight() > 40 ? 6 : 0);

    if (meshSelector != nullptr) {
        meshSelector->setBounds(bounds.removeFromTop(24).reduced((bounds.getWidth() - 24) / 2, 0));
	}
}

void PanelControls::addSlider(HSlider* slider) {
    sliders.add(slider);
    addAndMakeVisible(slider);
}

void PanelControls::initialise() {
}

void PanelControls::populateScratchSelector() {
    scratchSelector.clear(dontSendNotification);

	String dash(L"\u2013");
	scratchSelector.addItem(dash, NullScratchChannel);

	int size = getObj(MeshLibrary).getGroup(LayerGroups::GroupScratch).size();

	for(int i = 0; i < size; ++i) {
		scratchSelector.addItem(String(i + 1), i + 1);
	}
}

void PanelControls::setScratchSelector(int channel) {
    int id = channel == CommonEnums::Null ? PanelControls::NullScratchChannel : channel + 1;
    scratchSelector.setSelectedId(id, dontSendNotification);
}

void PanelControls::comboBoxChanged(ComboBox* box) {
    if (box == &scratchSelector) {
        Panel* panel = interactor->panel;

		int selectedId = box->getSelectedId();
		int scratchChannel = (selectedId == NullScratchChannel) ? CommonEnums::Null : selectedId - 1;

		MeshLibrary::Properties* focusedProps = getObj(MeshLibrary).getCurrentProps(interactor->layerType);
		focusedProps->scratchChan = scratchChannel;

		getObj(EditWatcher).setHaveEditedWithoutUndo(true);
		getObj(MeshLibrary).layerChanged(LayerGroups::GroupScratch, scratchChannel);

		panel->setSpeedApplicable(false);

        if (scratchChannel != CommonEnums::Null) {
            MeshLibrary::EnvProps* props = getObj(MeshLibrary).getEnvProps(LayerGroups::GroupScratch, scratchChannel);
            panel->setSpeedApplicable(props->active);
        }

        if (Interactor * opposite = interactor->getOppositeInteractor()) {
            opposite->panel->setSpeedApplicable(panel->isSpeedApplicable());
        }

		interactor->triggerRefreshUpdate();

		triggerAsyncUpdate();
	}
}

void PanelControls::handleAsyncUpdate() {
    getObj(MainPanel).grabKeyboardFocus();
}


void PanelControls::addEnablementIcon() {
    enableContainer = new DynamicSizeContainer(&enableCurrent, 27, 25);
    enableContainer->outline = true;
	enableContainer->translateUp1 = true;

	leftItems.add(&spacer1);
	leftItems.add(enableContainer.get());
	leftItems.add(&spacer6);
	addAndMakeVisible(&enableCurrent);

	haveEnablement = true;
}

void PanelControls::addMeshSelector(Component* selector) {
    this->meshSelector = selector;
    addAndMakeVisible(selector);
}

void PanelControls::addScratchSelector() {
    scratchSelector.setColour(ComboBox::outlineColourId, Colours::black);
	scratchSelector.addListener(this);
	scratchSelector.setWantsKeyboardFocus(false);

	scratchContainer = std::make_unique<DynamicSizeContainer>(&scratchSelector, 26, 26);
	scratchListener = std::make_unique<MouseOverMessager>(repo, "Set scratch channel", &scratchSelector);

	scratchContainer->width = 26;
	leftItems.add(scratchContainer.get());
	leftItems.add(&spacer2);
	leftItems.add(&scratchLabel);

	addAndMakeVisible(&scratchSelector);
	addAndMakeVisible(&scratchLabel);

	scratchSelector.addListener(this);
}

void PanelControls::addLayerItems(LayerSelectionClient* client, bool includeMover, bool separate) {
    layerSelector = std::make_unique<LayerSelectorPanel>(repo, client);
    layerSelector->outline = true;

	addAndMakeVisible(&addRemover);
	addRightItem(&addRemover, true);
	rightItems.add(&spacer8);

	if(includeMover) {
        addAndMakeVisible(&upDownMover);
        addRightItem(&upDownMover, true);
		rightItems.add(&spacer6);
	}

	Array<IDynamicSizeComponent*>& arr = separate ? leftItems : rightItems;
	arr.add(layerSelector.get());
	arr.add(&spacer2);

	addAndMakeVisible(layerSelector.get());
}

void PanelControls::addLeftItem(IDynamicSizeComponent* item, bool outline) {
    item->outline = outline;
    leftItems.add(item);
}

void PanelControls::addRightItem(IDynamicSizeComponent* item, bool outline) {
    item->outline = outline;
    rightItems.add(item);
}

void PanelControls::addDomainItems(RetractableCallout* domainCO) {
    haveDomains = true;
    this->domainCO = domainCO;
}

void PanelControls::resetSelector() {
    if (layerSelector != nullptr) {
	    layerSelector->reset();
    }
}

void PanelControls::refreshSelector(bool update) {
    if (layerSelector != nullptr) {
	    layerSelector->refresh(update);
    }
}

void PanelControls::moveLayer(bool up) {
    if (layerSelector != nullptr) {
	    layerSelector->moveCurrentLayer(up);
    }
}

