#ifndef GRAPHICRASTERIZER_H_
#define GRAPHICRASTERIZER_H_

#include <Curve/MeshRasterizer.h>
#include "../Updating/DynamicDetailUpdater.h"

class GraphicRasterizer :
		public MeshRasterizer
	,	public DynamicDetailUpdateable
{
public:
	GraphicRasterizer(SingletonRepo* repo,
					  Interactor* interactor,
					  const String& name, int layerGroup,
					  bool cyclic, float margin);

	virtual ~GraphicRasterizer();

	float& getPrimaryDimensionVar();
	void pullModPositionAndAdjust();

	Interactor* getInteractor() { return interactor; }

private:
	int layerGroup;
	Interactor* interactor;
};

typedef GraphicRasterizer TimeRasterizer;
typedef GraphicRasterizer SpectRasterizer;
typedef GraphicRasterizer PhaseRasterizer;


#endif
