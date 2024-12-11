#include "EnvelopeInter3D.h"
#include "../Inter/EnvelopeInter2D.h"

EnvelopeInter3D::EnvelopeInter3D(SingletonRepo* repo) :
        Interactor3D(repo, "EnvelopeInter3D", Dimensions(Vertex::Red, Vertex::Phase, Vertex::Blue)),
        SingletonAccessor(repo, "EnvelopeInter3D") {
}

void EnvelopeInter3D::init() {
    ignoresTime = true;
    scratchesTime = false;
    updateSource = UpdateSources::SourceEnvelope3D;
    layerType = LayerGroups::GroupVolume;
    collisionDetector.setNonintersectingDimension(CollisionDetector::Key);

    vertexProps.isEnvelope = true;
    vertexProps.ampVsPhaseApplicable = true;
    vertexProps.sliderApplicable[Vertex::Time] = false;
    vertexProps.dimensionNames.set(Vertex::Phase, "Time");
    vertexProps.dimensionNames.set(Vertex::Time, String::empty);
}

void EnvelopeInter3D::transferLineProperties(VertCube* from, VertCube* to1, VertCube* to2) {
    getObj(EnvelopeInter2D).transferLineProperties(from, to1, to2);
}

Mesh* EnvelopeInter3D::getMesh() {
    return getObj(EnvelopeInter2D).getMesh();
}
