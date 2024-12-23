#pragma once

#include <map>
#include <Curve/EnvRasterizer.h>
#include <Inter/Interactor2D.h>
#include <Obj/Ref.h>
#include <UI/Widgets/IconButton.h>
#include "../UI/Widgets/Controls/LayerAddRemover.h"
#include "../UI/Widgets/Controls/LayerSelectionClient.h"
#include "../UI/Widgets/Controls/LayerSelectorPanel.h"
#include "../UI/Widgets/Controls/MeshSelectionClient.h"

using std::map;

class Updater;
class SingletonRepo;
class EnvelopeMesh;
class Envelope2D;
class EnvRasterizer;
class SampleWrapper;

class EnvelopeInter2D :
        public Interactor2D,
        public Button::Listener,
        public MeshSelectionClient<EnvelopeMesh>,
        public LayerSelectionClient {
public:
    enum {
        CfgDynamic = 1,

        CfgSyncTempo,
        CfgGlobal,
        CfgLogarithmic,

        CfgScale1_16x,
        CfgScale1_4x,
        CfgScale1_2x,
        CfgScale1x,
        CfgScale2x,
        CfgScale4x,
        CfgScale16x,
    };

    explicit EnvelopeInter2D(SingletonRepo* repo);

    void init() override;
    void performUpdate(UpdateType updateType) override;
    bool doesMeshChangeWarrantGlobalUpdate() override;

    void showCoordinates() override;
    int getUpdateSource() override;

    /* envelope functions */

    /* callbacks */
    void buttonClicked(Button* button) override;

    /* selection client */
    String getDefaultFolder() override;

    Mesh* getMesh() override;
    EnvelopeMesh* getCurrentMesh() override;
    MeshRasterizer* getRast(int envEnum);
    EnvRasterizer* getEnvRasterizer();
    vector<VertCube*> getLinesToSlideOnSingleSelect() override;
    Button* getEnableButton() { return &enableButton; }

    bool addNewCube(float startTime, float x, float y, float curve) override;
    bool isCurrentMeshActive() override;
    bool isPitchEnvelope();
    bool shouldDoDimensionCheck() override;
    bool synchronizeEnvPoints(Vertex* startVertex, bool vertexIsLoopVert);
    Range<float> getVertexPhaseLimits(Vertex* vert) override;
    void adjustAddedLine(VertCube* addedLine) override;
    void delegateUpdate(bool performUpdate);
    void doExtraMouseDrag(const MouseEvent& e) override;
    void doExtraMouseUp() override;
    void doSustainReleaseChange(bool isSustain);
    void enablementsChanged();
    void enterClientLock() override;
    void exitClientLock() override;
    void layerChanged() override;
    void previewMesh(EnvelopeMesh* mesh) override;
    void previewMeshEnded(EnvelopeMesh* oldMesh) override;
    void removeCurrentEnvLine(bool isLoop);
    void reset() override;
    void setCurrentMesh(EnvelopeMesh* mesh) override;
    void setEnabledHighlight(bool highlit);
    void switchedEnvelope(int envMesh, bool doUpdate = true, bool force = false);
    void syncEnvPointsImplicit();
    void toggleEnvelopePoint(Button* button);
    void transferLineProperties(VertCube* from, VertCube* to1, VertCube* to2);
    void triggerButton(int id);
    void updateHighlights();
    void updatePhaseLimit(float limit);
    void validateMesh() override;
    void waveOverlayChanged();

    int getLayerType() override;
    LayerSelectorPanel* getScratchSelector() { return &layerSelector; }

    void chooseConfigScale(int result, MeshLibrary::EnvProps* props);

private:
    friend class Envelope2D;
    friend class Updater;

    Ref<Envelope2D> envPanel;

    IconButton volumeIcon;
    IconButton pitchIcon;
    IconButton wavePitchIcon;
    IconButton scratchIcon;
    IconButton configIcon;

    IconButton loopIcon;
    IconButton sustainIcon;
    IconButton enableButton;

    IconButton contractIcon;
    IconButton expandIcon;

    LayerSelectorPanel layerSelector;
    LayerAddRemover addRemover;

    Mesh backupMesh;
    map<int, int> meshToSlider;
};