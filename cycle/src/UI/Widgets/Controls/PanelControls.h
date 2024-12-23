#pragma once

#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/Widgets/DynamicLabel.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/RetractableCallout.h>
#include <UI/Layout/DynamicSizeContainer.h>
#include <UI/MouseOverMessager.h>

#include "LayerAddRemover.h"
#include "LayerSelectorPanel.h"
#include "LayerUpDownMover.h"
#include "Spacers.h"
#include "../HSlider.h"

class EnvelopeMesh;
class LayerSelectorPanel;
class IDynamicSizeComponent;
class Mesh;

class PanelControls :
        public ComboBox::Listener
    ,	public Component
    , 	public SingletonAccessor
    , 	public AsyncUpdater {
public:
    enum { NullScratchChannel = 1000 };

    PanelControls(SingletonRepo* 		repo,
                  Interactor*			itr,
                  Button::Listener* 	listener,
                  LayerSelectionClient* client 		= nullptr,
                  const String& 		layerString = {});

    void paint(Graphics& g) override;
    void addSlider(HSlider* slider);
    void comboBoxChanged(ComboBox* box) override;
    void setScratchSelector(int channel);

    void resized() override;
    void initialise();

    void populateScratchSelector();
    void addScratchSelector();
    void addMeshSelector(Component* selector);
    void addEnablementIcon();
    void addLayerItems(LayerSelectionClient* client, bool includeMover, bool separate = false);
    void addDomainItems(RetractableCallout* domainCO);

    void addLeftItem(IDynamicSizeComponent* item, bool outline = false);
    void addRightItem(IDynamicSizeComponent* item, bool outline = false);

    void resetSelector() const;
    void refreshSelector(bool update = false) const;
    void moveLayer(bool up) const;
    void handleAsyncUpdate() override;

    bool haveEnablement, drawLabel, haveDomains;

    Image 			icon;
    ComboBox 		scratchSelector;
    DynamicLabel 	scratchLabel;
    IconButton 		enableCurrent;
    Component* 		meshSelector;

    ClearSpacer spacer1, spacer2, spacer4, spacer6, spacer8;
    Rectangle<int> iconBounds, layerLblBounds, domainBounds;

    Array<HSlider*> sliders;

    LayerAddRemover addRemover;
    LayerUpDownMover upDownMover;

    Ref<Interactor> 				interactor;
    Ref<LayerSelectionClient> 		layerClient;
    Ref<RetractableCallout>			domainCO;

    Array<IDynamicSizeComponent*> 	leftItems;
    Array<IDynamicSizeComponent*> 	rightItems;

    std::unique_ptr<MouseOverMessager> 	scratchListener;
    std::unique_ptr<MouseOverMessager> 	layerListener;
    std::unique_ptr<LayerSelectorPanel>	layerSelector;
    std::unique_ptr<DynamicSizeContainer> enableContainer;
    std::unique_ptr<DynamicSizeContainer> scratchContainer;
};
