#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/MorphDimensions.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

#include "Nodes/Envelope/EnvelopeEditCore.h"

namespace CycleV2 {

int EnvelopeEditCore::fieldDimension(const juce::String& field) {
    const auto name = field.containsChar('.') ? field.fromLastOccurrenceOf(".", false, false) : field;
    if (name == "time" || name == "yellow" || name == "component") {
        return Vertex::Time;
    }
    if (name == "red") {
        return Vertex::Red;
    }
    if (name == "blue") {
        return Vertex::Blue;
    }
    if (name == "phase") {
        return Vertex::Phase;
    }
    if (name == "amp") {
        return Vertex::Amp;
    }
    return name == "curve" ? Vertex::Curve : -1;
}

juce::Array<Vertex*> EnvelopeEditCore::linkedVertices(
        VertCube* cube, Vertex* vertex, const EnvelopeEditScope& scope) {
    juce::Array<Vertex*> result;
    if (cube == nullptr || vertex == nullptr || scope.dimensionCount < 0 || scope.dimensionCount > 3) {
        return result;
    }
    const int selected = cube->indexOf(vertex);
    if (selected < 0) {
        return result;
    }
    int linked = ~MorphDimensions(scope.dimensionCount).cornerMask() & 7;
    for (size_t axis = 0; axis < scope.linkedAxes.size(); ++axis) {
        if (scope.linkedAxes[axis]) {
            linked |= MorphDimensions::poleBits[axis];
        }
    }
    for (int corner = 0; corner < VertCube::numVerts; ++corner) {
        if ((corner & ~linked) == (selected & ~linked)) {
            result.addIfNotAlreadyThere(cube->getVertex(corner));
        }
    }
    return result;
}

bool EnvelopeEditCore::setValue(VertCube& cube, int corner, int dimension,
        float value, const EnvelopeEditScope& scope, bool* changed) {
    if (corner < 0 || corner >= VertCube::numVerts || dimension < 0 || dimension >= Vertex::numElements
            || scope.dimensionCount < 0 || scope.dimensionCount > 3 || !std::isfinite(value)
            || value < 0.f || value > (dimension == Vertex::Phase ? maximumPhase : 1.f)) {
        return false;
    }
    const bool morph = MorphDimensions::cornerBit(dimension) != 0;
    if (morph && !MorphDimensions(scope.dimensionCount).active(dimension)) {
        return false;
    }
    auto effectiveScope = scope;
    if (morph) {
        effectiveScope.linkedAxes = { false, false, false };
    }
    for (auto* vertex : linkedVertices(&cube, cube.getVertex(corner), effectiveScope)) {
        if (changed != nullptr) {
            *changed |= vertex->values[dimension] != value;
        }
        vertex->values[dimension] = value;
    }
    return true;
}

bool EnvelopeEditCore::setGuideGain(VertCube& cube, int dimension,
        float value, int dimensionCount, bool* changed) {
    if (dimension < 0 || dimension >= Vertex::numElements || dimensionCount < 0 || dimensionCount > 3
            || !std::isfinite(value) || value < 0.f || value > 1.f
            || ((dimension == Vertex::Red || dimension == Vertex::Blue)
                    && !MorphDimensions(dimensionCount).active(dimension))) {
        return false;
    }
    if (changed != nullptr) {
        *changed |= cube.guideCurveGainAt(dimension) != value;
    }
    cube.guideCurveGainAt(dimension) = value;
    return true;
}

bool EnvelopeEditCore::markerState(const EnvelopeMesh& mesh, const VertCube& cube, bool loop) {
    const auto& markers = loop ? mesh.loopCubes : mesh.sustainCubes;
    return markers.find(const_cast<VertCube*>(&cube)) != markers.end();
}

bool EnvelopeEditCore::setMarker(EnvelopeMesh& mesh, VertCube& cube,
        bool loop, bool enabled, EnvRasterizer& rasterizer) {
    if (markerState(mesh, cube, loop) == enabled) {
        return false;
    }
    auto& markers = loop ? mesh.loopCubes : mesh.sustainCubes;
    markers.clear();
    if (enabled) {
        markers.insert(&cube);
    }
    rasterizer.evaluateLoopSustainIndices();
    synchronizeLoopSeam(mesh, rasterizer, loop);
    return true;
}

bool EnvelopeEditCore::validateMarkers(EnvelopeMesh& mesh, EnvRasterizer& rasterizer) {
    const std::set<VertCube*> live(mesh.getCubes().begin(), mesh.getCubes().end());
    bool changed = false;
    for (auto* markers : { &mesh.loopCubes, &mesh.sustainCubes }) {
        for (auto it = markers->begin(); it != markers->end();) {
            if (live.find(*it) == live.end()) {
                it = markers->erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
    }
    rasterizer.evaluateLoopSustainIndices();
    int loopIndex = -1;
    int sustainIndex = -1;
    rasterizer.getIndices(loopIndex, sustainIndex);
    if (sustainIndex >= 0) {
        const auto snapshot = rasterizer.snapshotView();
        const auto& intercepts = snapshot.intercepts();
        for (int index = 0; index < (int) intercepts.size(); ++index) {
            auto* cube = intercepts[(size_t) index].cube;
            if (cube != nullptr && mesh.loopCubes.find(cube) != mesh.loopCubes.end()
                    && index > sustainIndex - EnvRasterizer::loopMinSizeIcpts) {
                changed |= mesh.loopCubes.erase(cube) != 0;
            }
        }
    }
    rasterizer.evaluateLoopSustainIndices();
    return changed;
}

bool EnvelopeEditCore::synchronizeLoopSeam(
        EnvelopeMesh& mesh, EnvRasterizer& rasterizer, bool sourceIsLoop) {
    juce::ignoreUnused(mesh);
    int loopIndex = -1;
    int sustainIndex = -1;
    rasterizer.getIndices(loopIndex, sustainIndex);
    const auto snapshot = rasterizer.snapshotView();
    const auto& intercepts = snapshot.intercepts();
    auto* loop = loopIndex >= 0 && loopIndex < (int) intercepts.size()
            ? intercepts[(size_t) loopIndex].cube : nullptr;
    auto* sustain = sustainIndex >= 0 && sustainIndex < (int) intercepts.size()
            ? intercepts[(size_t) sustainIndex].cube : nullptr;
    bool changed = false;
    if (loop != nullptr && sustain != nullptr) {
        auto* target = sourceIsLoop ? sustain : loop;
        auto* source = sourceIsLoop ? loop : sustain;
        for (int corner = 0; corner < VertCube::numVerts; ++corner) {
            auto* destination = target->getVertex(corner);
            auto* origin = source->getVertex(corner);
            changed |= destination->values[Vertex::Amp] != origin->values[Vertex::Amp];
            destination->values[Vertex::Amp] = origin->values[Vertex::Amp];
            for (const auto& pair : { std::make_pair(target, destination), std::make_pair(source, origin) }) {
                if (pair.first->getCompGuideCurve() < 0) {
                    const float before = pair.second->values[Vertex::Curve];
                    pair.second->setMaxSharpness();
                    changed |= before != pair.second->values[Vertex::Curve];
                }
            }
        }
    } else if (sustain != nullptr && sustain->getCompGuideCurve() < 0) {
        for (int corner = 0; corner < VertCube::numVerts; ++corner) {
            auto* vertex = sustain->getVertex(corner);
            const float before = vertex->values[Vertex::Curve];
            vertex->setMaxSharpness();
            changed |= before != vertex->values[Vertex::Curve];
        }
    }
    return changed;
}

void EnvelopeEditCore::transferMarkers(
        EnvelopeMesh& mesh, const VertCube& source, VertCube& first, VertCube& second) {
    for (auto* markers : { &mesh.loopCubes, &mesh.sustainCubes }) {
        if (markers->erase(const_cast<VertCube*>(&source)) != 0) {
            markers->insert(&first);
            markers->insert(&second);
        }
    }
}

}
