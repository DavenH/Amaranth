#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Inter/Interactor.h>
#include <Definitions.h>
#include "GraphicRasterizer.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/VisualDsp.h"

// New pipeline components
#include <Wireframe/Rasterizer/MeshRasterizer.h>
#include <Wireframe/Interpolator/Trilinear/TrilinearInterpolator.h>
#include <Wireframe/Positioner/CyclicPointPositioner.h>
#include <Wireframe/Positioner/PointPositioner.h>
#include <Wireframe/Positioner/PathDeformingPositioner.h>
#include <Wireframe/Curve/SimpleCurveGenerator.h>
#include <Wireframe/Sampler/SimpleCurveSampler.h>
#include <Wireframe/State/RasterizerParameters.h>
#include <Array/VecOps.h>

GraphicRasterizer::GraphicRasterizer(
            SingletonRepo* repo
        ,   Interactor* interactor
        ,   const String& name
        ,   int layerGroup
        ,   bool isCyclic
        ,   float margin) : SingletonAccessor(repo, name)
    ,   OldMeshRasterizer(name)
    ,   layerGroup(layerGroup)
    ,   interactor(interactor) {
    cyclic = isCyclic;
    addListener(this);
    setLimits(-margin, 1 + margin);
}

// Override to use the new pipeline: TrilinearInterpolator → CyclicPointPositioner →
// PathDeformingPositioner → SimpleCurveGenerator → SimpleCurveSampler
void GraphicRasterizer::generateControlPoints() {
    Mesh* used = getMesh();
    if (!used || used->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    MeshRasterizer<Mesh*> pipeline(
        std::make_unique<TrilinearInterpolator>(used),
        std::make_unique<SimpleCurveGenerator>()
    );

    if (wrapsVertices()) {
        auto cyclicPos = std::make_unique<CyclicPointPositioner>();
        cyclicPos->setInterceptPadding(interceptPadding);
        pipeline.addPositioner(std::move(cyclicPos));
    } else {
        pipeline.addPositioner(std::make_unique<PointPositioner>());
    }

    if (auto* p = getPath()) {
        auto pathPos = std::make_unique<PathDeformingPositioner>();
        pathPos->setPath(p);
        pathPos->setMorph(getMorphPosition());
        pathPos->setReduction(reduct);
        pathPos->setNoiseSeed(noiseSeed);
        pathPos->setPhaseSeeds(phaseOffsetSeeds, 128);
        pathPos->setVertSeeds(vertOffsetSeeds, 128);
        // Match legacy default scaling behavior (OldMeshRasterizer default: Bipolar)
        pathPos->setScalingType(Bipolar);
        pipeline.addPositioner(std::move(pathPos));
    }

    CurveParameters cfg;
    cfg.cyclic = wrapsVertices();
    cfg.minX = xMinimum;
    cfg.maxX = xMaximum;

    curves = pipeline.run(used, cfg);

    // Legacy resolution index assignment and curve recalc
    float base = 0.1f / float(CurvePiece::resolution);
    setResolutionIndices(base);
    for (auto& c : curves) c.recalculateCurve();

    // Sample curves to waveform arrays
    SimpleCurveSampler sampler;
    sampler.buildFromCurves(curves);

    int size = sampler.getWaveX().size();
    updateBuffers(size);

    sampler.getWaveX().copyTo(waveX);
    sampler.getWaveY().copyTo(waveY);
    sampler.getSlope().copyTo(slope);

    if (size > 1) {
        Buffer<float> dif = diffX.withSize(size - 1);
        Buffer<float> slp = slope.withSize(size - 1);
        VecOps::sub(waveX, waveX + 1, dif);
        dif.threshLT(1e-6f);
        VecOps::sub(waveY, waveY + 1, slp);
        slp.div(dif);
    }

    zeroIndex = sampler.getZeroIndex();
    oneIndex  = sampler.getOneIndex();
    unsampleable = !sampler.isSampleable();
}

void GraphicRasterizer::pullModPositionAndAdjust() {
    morph = getObj(MorphPanel).getMorphPosition();

    MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentProps(layerGroup);

    if (props->scratchChan != CommonEnums::Null) {
        morph.time = getObj(VisualDsp).getScratchPosition(props->scratchChan);
    }
}

int GraphicRasterizer::getPrimaryViewDimension() {
    return getSetting(CurrentMorphAxis);
}
