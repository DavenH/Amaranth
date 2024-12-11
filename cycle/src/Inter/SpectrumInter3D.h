#pragma once

#include <Inter/Interactor3D.h>
#include "../UI/Widgets/Controls/MeshSelectionClient3D.h"

class SpectrumInter3D:
		public Interactor3D
	,	public SelectionClientOwner
{
public:
	explicit SpectrumInter3D(SingletonRepo* repo);

	bool isCurrentMeshActive() override;
	int getTableIndexY(float y, int size) override;

	Interactor* getOppositeInteractor() override;
	String getYString(float yVal, int yIndex, const Column& col, float fundFreq) override;
	String getZString(float tableValue, int xIndex) override;

	void doExtraMouseUp() override;
	void enterClientLock(bool audioThreadApplicable) override;
	void exitClientLock(bool audioThreadApplicable) override;

	void init() override;
	void initSelectionClient();
	void meshSelectionChanged(Mesh* mesh) override;
	void meshSelectionFinished() override;
	void updateRastDims() override;
	void updateSelectionClient();
};