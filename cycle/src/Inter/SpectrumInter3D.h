#ifndef _f3interactor
#define _f3interactor

#include <math.h>
#include "JuceHeader.h"
#include <Inter/Interactor3D.h>
#include "../UI/Widgets/Controls/MeshSelectionClient3D.h"

class SpectrumInter3D:
		public Interactor3D
	,	public SelectionClientOwner
{
public:
	SpectrumInter3D(SingletonRepo* repo);
	virtual ~SpectrumInter3D();

	bool isCurrentMeshActive();
	int getTableIndexY(float y, int size);

	Interactor* getOppositeInteractor();
	String getYString(float yVal, int yIndex, const Column& col, float fundFreq);
	String getZString(float tableValue, int xIndex);

	void doExtraMouseUp();
	void enterClientLock(bool audioThreadApplicable);
	void exitClientLock(bool audioThreadApplicable);

	void init();
	void initSelectionClient();
	void meshSelectionChanged(Mesh* mesh);
	void meshSelectionFinished();
	void updateRastDims();
	void updateSelectionClient();
};

#endif
