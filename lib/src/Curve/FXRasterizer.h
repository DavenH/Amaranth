#pragma once

#include "MeshRasterizer.h"

class FXRasterizer: public MeshRasterizer {
private:
	JUCE_LEAK_DETECTOR(FXRasterizer)

public:
	explicit FXRasterizer(const String& name = String());
	bool hasEnoughCubesForCrossSection() override;
	int  getNumDims() override;
	void calcCrossPoints() override;
	void cleanUp() override;
	void padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) override;
	void setMesh(Mesh* newMesh) override;
};
