#pragma once

#include <App/SingletonAccessor.h>

#include "MeshRasterizer.h"

class FXRasterizer:
		public MeshRasterizer
	,	public SingletonAccessor {
	JUCE_LEAK_DETECTOR(FXRasterizer)

public:
	explicit FXRasterizer(SingletonRepo* repo, const String& name = String());
	bool hasEnoughCubesForCrossSection() override;
	int  getNumDims() override;
	void calcCrossPoints(int currentDim) override;
	void cleanUp() override;
	void padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) override;
	void setMesh(Mesh* newMesh) override;
};
