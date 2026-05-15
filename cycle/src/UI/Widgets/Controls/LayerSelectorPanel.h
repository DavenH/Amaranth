#pragma once
#include <Obj/Ref.h>
#include "SelectorPanel.h"

class SingletonRepo;
class LayerSelectionClient;

class LayerSelectorPanel :
    public SelectorPanel {
public:
    LayerSelectorPanel(SingletonRepo* repo, LayerSelectionClient* client);

    ~LayerSelectorPanel() override = default;

    void moveCurrentLayer(bool up);

    int getSize() override;
    int getCurrentIndexExternal() override;
    void selectionChanged() override;
    void rowClicked(int row) override;
    float getWheelStepThreshold() const override { return 0.42f; }

protected:
    Ref<LayerSelectionClient> client;

};
