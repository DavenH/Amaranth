#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Inter/Interactor.h>

#include "GraphicRasterizer.h"

#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/VisualDsp.h"

GraphicRasterizer::GraphicRasterizer(SingletonRepo* repo, Interactor* interactor,
									 const String& name, int layerGroup,
									 bool isCyclic, float margin) :
		MeshRasterizer(repo, name)
	,	layerGroup(layerGroup)
	, 	interactor(interactor) {
	cyclic = isCyclic;
	addListener(this);
	setLimits(-margin, 1 + margin);
}


GraphicRasterizer::~GraphicRasterizer() {
}


void GraphicRasterizer::pullModPositionAndAdjust() {
    morph = getObj(MorphPanel).getMorphPosition();

    MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentProps(layerGroup);

	if(props->scratchChan != CommonEnums::Null)
		morph.time = getObj(VisualDsp).getScratchPosition(props->scratchChan);
}


float &GraphicRasterizer::getPrimaryDimensionVar() {
	int xDim = getSetting(CurrentMorphAxis);
	return morph[xDim];
}
