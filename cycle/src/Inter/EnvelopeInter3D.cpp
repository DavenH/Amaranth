#include "EnvelopeInter3D.h"

#include <Util/Util.h>

#include "../Inter/EnvelopeInter2D.h"
#include "../Util/CycleEnums.h"

EnvelopeInter3D::EnvelopeInter3D(SingletonRepo* repo) :
        Interactor3D(repo, Util::className(this), Dimensions(Vertex::Red, Vertex::Phase, Vertex::Blue)),
        SingletonAccessor(repo, Util::className(this)) {
}

void EnvelopeInter3D::init() {
    Interactor3D::init();

    ignoresTime   = true;
    scratchesTime = false;
    updateSource  = UpdateSources::SourceEnvelope3D;
    layerType     = LayerGroups::GroupVolume;
    collisionDetector.setNonintersectingDimension(CollisionDetector::Key);

    vertexProps.isEnvelope                     = true;
    vertexProps.ampVsPhaseApplicable           = true;
    vertexProps.sliderApplicable[Vertex::Time] = false;
    vertexProps.dimensionNames.set(Vertex::Phase, "Time");
    vertexProps.dimensionNames.set(Vertex::Time, {});
}

void EnvelopeInter3D::transferLineProperties(TrilinearCube* from, TrilinearCube* to1, TrilinearCube* to2) {
    getObj(EnvelopeInter2D).transferLineProperties(from, to1, to2);
}

Mesh* EnvelopeInter3D::getMesh() {
    return getObj(EnvelopeInter2D).getMesh();
}
