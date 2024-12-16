#pragma once

#include <Curve/MeshRasterizer.h>
#include "../Updating/DynamicDetailUpdater.h"

class GraphicRasterizer :
		public MeshRasterizer
	,	public DynamicDetailUpdateable
	,	public SingletonAccessor
{
public:
	GraphicRasterizer(SingletonRepo* repo,
					  Interactor* interactor,
					  const String& name, int layerGroup,
					  bool cyclic, float margin);

	~GraphicRasterizer() override;

	float& getPrimaryDimensionVar() override;
	void pullModPositionAndAdjust() override;

	Interactor* getInteractor() const { return interactor; }

private:
	int layerGroup;
	Interactor* interactor;
};

typedef GraphicRasterizer TimeRasterizer;
typedef GraphicRasterizer SpectRasterizer;
typedef GraphicRasterizer PhaseRasterizer;
