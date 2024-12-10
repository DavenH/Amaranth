#ifndef _surfaceinteractor_h
#define _surfaceinteractor_h

#include <Inter/Interactor3D.h>
#include "../UI/Widgets/Controls/MeshSelectionClient3D.h"

class WaveformInter3D :
	public Interactor3D,
    public SelectionClientOwner {
private:
	float shiftedPhase;

public:
	WaveformInter3D(SingletonRepo* repo);
	virtual ~WaveformInter3D() {}

	void init();
	void doPhaseShift(float shift);
	void doCommitPencilEditPath();
	void initSelectionClient();
	void meshSelectionChanged(Mesh* mesh);
	bool isCurrentMeshActive();
	void doExtraMouseUp();
	void meshSelectionFinished();
	void updateRastDims();
	Interactor* getOppositeInteractor();
	void enterClientLock(bool audioThreadApplicable);
	void exitClientLock(bool audioThreadApplicable);
	String getYString(float yVal, int yIndex, const Column& col, float fundFreq);
};

#endif
