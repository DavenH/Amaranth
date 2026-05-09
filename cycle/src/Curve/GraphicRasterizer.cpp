#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Inter/Interactor.h>
#include "GraphicRasterizer.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/VisualDsp.h"
#include <Definitions.h>

GraphicRasterizer::GraphicRasterizer(
            SingletonRepo* repo
        ,   Interactor* interactor
        ,   const String& name
	        ,   int layerGroup
	        ,   bool isCyclic
	        ,   float margin) : SingletonAccessor(repo, name)
	    ,   MeshRasterizer(name)
	    ,   layerGroup(layerGroup)
	    ,   interactor(interactor) {
    setWrapsEnds(isCyclic);
    addListener(this);
    setLimits(-margin, 1 + margin);
    setPrimaryViewDimensionProvider([repo]() {
        return Cycle::Rasterization::GraphicRasterizerFacade().primaryViewDimension(
                repo->get<Settings>("Settings").getGlobalSetting(AppSettings::CurrentMorphAxis));
    });
}

void GraphicRasterizer::pullModPositionAndAdjust() {
    Cycle::Rasterization::GraphicMorphPositionPolicy::Context context;
    context.panelMorph = getObj(MorphPanel).getMorphPosition();
    context.layerGroup = layerGroup;
    context.currentMorphAxis = getSetting(CurrentMorphAxis);

    MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentProps(layerGroup);

    if (props == nullptr || props->scratchChan == CommonEnums::Null) {
        setMorphPosition(Cycle::Rasterization::GraphicRasterizerFacade().resolveMorphPosition(context));
        return;
    }

    context.scratchChannel = props->scratchChan;
    context.scratchPosition = getObj(VisualDsp).getScratchPosition(props->scratchChan);
    setMorphPosition(Cycle::Rasterization::GraphicRasterizerFacade().resolveMorphPosition(context));
}
