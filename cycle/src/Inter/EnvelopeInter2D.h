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

    EnvelopeInter2D(SingletonRepo* repo);
    virtual ~EnvelopeInter2D();

    void init();
    void performUpdate(int updateType);
    bool doesMeshChangeWarrantGlobalUpdate();

    void showCoordinates();
    int getUpdateSource();

    /* envelope functions */

    /* callbacks */
    void buttonClicked(Button* button);

    /* selection client */
    String getDefaultFolder();

    Mesh* getMesh();
    EnvelopeMesh* getCurrentMesh();
    MeshRasterizer* getRast(int envEnum);
    EnvRasterizer* getEnvRasterizer();
    vector<VertCube*> getLinesToSlideOnSingleSelect();
    Button* getEnableButton() { return &enableButton; }

    bool addNewCube(float startTime, float x, float y, float curve);
    bool isCurrentMeshActive();
    bool isPitchEnvelope();
    bool shouldDoDimensionCheck();
    bool synchronizeEnvPoints(Vertex* startVertex, bool vertexIsLoopVert);
    Range<float> getVertexPhaseLimits(Vertex* vert);
    void adjustAddedLine(VertCube* addedLine);
    void delegateUpdate(bool performUpdate);
    void doExtraMouseDrag(const MouseEvent& e);
    void doExtraMouseUp();
    void doSustainReleaseChange(bool isSustain);
    void enablementsChanged();
    void enterClientLock();
    void exitClientLock();
    void layerChanged();
    void previewMesh(EnvelopeMesh* mesh);
    void previewMeshEnded(EnvelopeMesh* oldMesh);
    void removeCurrentEnvLine(bool isLoop);
    void reset();
    void setCurrentMesh(EnvelopeMesh* mesh);
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

    int getLayerType();
    LayerSelectorPanel* getScratchSelector() { return &layerSelector; }

private:
    friend class Envelope2D;
    friend class Updater;

    Ref <Envelope2D> envPanel;

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