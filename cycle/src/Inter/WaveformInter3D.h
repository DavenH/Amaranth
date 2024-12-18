#pragma once

#include <Inter/Interactor3D.h>
#include "../UI/Widgets/Controls/MeshSelectionClient3D.h"

class WaveformInter3D :
    public Interactor3D,
    public SelectionClientOwner {
private:
    float shiftedPhase;

public:
    explicit WaveformInter3D(SingletonRepo* repo);

    ~WaveformInter3D() override = default;

    void init() override;
    void doPhaseShift(float shift);
    void doCommitPencilEditPath() override;
    void initSelectionClient();
    void meshSelectionChanged(Mesh* mesh) override;
    bool isCurrentMeshActive() override;
    void doExtraMouseUp() override;
    void meshSelectionFinished() override;
    void updateRastDims() override;
    Interactor* getOppositeInteractor() override;
    void enterClientLock(bool audioThreadApplicable) override;
    void exitClientLock(bool audioThreadApplicable) override;
    String getYString(float yVal, int yIndex, const Column& col, float fundFreq) override;
};