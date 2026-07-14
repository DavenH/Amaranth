#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Inter/Interactor.h>
#include <UI/Panels/Morphing/MorphPanel.h>
#include <UI/VisualDsp.h>
#include <Definitions.h>

#include "GraphicRasterizer.h"

GraphicRasterizer::GraphicRasterizer(
        SingletonRepo* repo,
        Interactor* interactor,
        const String& name,
        int layerGroup,
        bool cyclic,
        float margin) :
        SingletonAccessor(repo, name)
    ,   Rasterization::GraphicRasterizer(cyclic, margin)
    ,   layerGroup(layerGroup)
    ,   interactor(interactor) {
    addListener(this);
}

void GraphicRasterizer::pullModPositionAndAdjust() {
    Cycle::Rasterization::GraphicMorphPositionPolicy::Context context;
    context.panelMorph = getObj(MorphPanel).getMorphPosition();
    context.layerGroup = layerGroup;
    context.currentMorphAxis = getSetting(CurrentMorphAxis);

    MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentProps(layerGroup);

    if (props == nullptr || props->scratchChan == CommonEnums::Null) {
        setMorphPosition(Cycle::Rasterization::GraphicMorphPositionPolicy().resolve(context));
        return;
    }

    context.scratchChannel = props->scratchChan;
    context.scratchPosition = getObj(VisualDsp).getScratchPosition(props->scratchChan);
    setMorphPosition(Cycle::Rasterization::GraphicMorphPositionPolicy().resolve(context));
}

void GraphicRasterizer::updateGeometry() {
    prepareRequestForRender();
    Rasterization::GraphicRasterizer::updateGeometry();
}

void GraphicRasterizer::updateGeometry(Mesh* mesh, float oscPhase) {
    prepareRequestForRender();
    Rasterization::GraphicRasterizer::updateGeometry(mesh, oscPhase);
}

void GraphicRasterizer::updateWaveform() {
    prepareRequestForRender();
    Rasterization::GraphicRasterizer::updateWaveform();
}

void GraphicRasterizer::updateWaveform(Mesh* mesh, float oscPhase) {
    prepareRequestForRender();
    Rasterization::GraphicRasterizer::updateWaveform(mesh, oscPhase);
}

void GraphicRasterizer::prepareRequestForRender() {
    int currentMorphAxis = repo->get<Settings>("Settings").getGlobalSetting(AppSettings::CurrentMorphAxis);
    getRequest().primaryViewDimension =
            Cycle::Rasterization::GraphicAxisPolicy().primaryViewDimension(currentMorphAxis);
}
