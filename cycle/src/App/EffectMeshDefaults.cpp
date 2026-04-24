#include "EffectMeshDefaults.h"

#include <JuceHeader.h>

#include <limits>

#include <Definitions.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>
#include <Curve/Mesh.h>
#include <Curve/Vertex.h>

#include "../Util/CycleEnums.h"

namespace {
    struct VertexRange {
        float min{};
        float max{};
    };

    VertexRange getVertexRange(Mesh* mesh, int dim) {
        VertexRange range { std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };

        for (auto* vert : mesh->getVerts()) {
            float value = vert->values[dim];
            range.min = jmin(range.min, value);
            range.max = jmax(range.max, value);
        }

        return range;
    }

    void remapVertexRange(Mesh* mesh, int dim, float minValue, float maxValue) {
        if (mesh == nullptr) {
            return;
        }

        float scale = maxValue - minValue;

        for (auto* vert : mesh->getVerts()) {
            vert->values[dim] = minValue + vert->values[dim] * scale;
        }
    }

    void initTubeModelMesh(SingletonRepo* repo, Mesh* mesh) {
        if (mesh == nullptr || mesh->getNumVerts() > 0) {
            return;
        }

        float padding = getRealConstant(IrModellerPadding);
        auto& verts = mesh->getVerts();

        verts.push_back(new Vertex(padding * 0.5f,  0.5f));
        verts.push_back(new Vertex(padding - 0.001f, 0.5f));
        verts.push_back(new Vertex(padding + 0.001f, 0.5f));
        verts.push_back(new Vertex(padding + 0.003f, 0.5f));
        verts.push_back(new Vertex(padding + 0.005f, 1.0f));
        verts.push_back(new Vertex(padding + 0.010f, 0.1313f));
        verts.push_back(new Vertex(padding + 0.1f,   0.6f));
        verts.push_back(new Vertex(padding + 0.15f,  0.5f));
        verts.push_back(new Vertex(padding + 0.2f,   0.5f));
        verts.push_back(new Vertex(1.f,              0.5f));
    }

    void initWaveshaperMesh(SingletonRepo* repo, Mesh* mesh) {
        if (mesh == nullptr || mesh->getNumVerts() > 0) {
            return;
        }

        float padding = getRealConstant(WaveshaperPadding);
        auto& verts = mesh->getVerts();

        verts.push_back(new Vertex(padding * 0.5f,       padding * 0.5f));
        verts.push_back(new Vertex(padding,              padding));
        verts.push_back(new Vertex(1.f - padding,        1.f - padding));
        verts.push_back(new Vertex(1.f - padding * 0.5f, 1.f - padding * 0.5f));

        for (auto* vert : verts) {
            vert->setMaxSharpness();
        }
    }

    void initGuideMesh(SingletonRepo* repo, Mesh* mesh) {
        if (mesh == nullptr || mesh->getNumVerts() > 0) {
            return;
        }

        float padding = getRealConstant(GuideCurvePadding);
        auto& verts = mesh->getVerts();

        verts.push_back(new Vertex(padding,       0.5f));
        verts.push_back(new Vertex(1.f - padding, 0.5f));

        verts[0]->setMaxSharpness();
        verts[1]->setMaxSharpness();
    }
}

void EffectMeshDefaults::initialiseIfNeeded(SingletonRepo* repo, int layerType, Mesh* mesh) {
    // TODO(daven): This legacy effect-mesh bootstrap is application-specific and
    // should move behind an explicit app extension hook instead of being invoked
    // ad hoc from cycle startup/UI code.
    switch (layerType) {
        case LayerGroups::GroupGuideCurve: initGuideMesh(repo, mesh); break;
        case LayerGroups::GroupWaveshaper: initWaveshaperMesh(repo, mesh); break;
        case LayerGroups::GroupIrModeller: initTubeModelMesh(repo, mesh); break;
        default: break;
    }
}

void EffectMeshDefaults::migrateLegacyPaddingIfNeeded(SingletonRepo* repo, int layerType, Mesh* mesh) {
    // TODO(daven): This app-specific migration should eventually live behind an
    // explicit app extension/migration hook rather than Cycle post-load code.
    if (mesh == nullptr || mesh->getNumVerts() == 0) {
        return;
    }

    switch (layerType) {
        case LayerGroups::GroupGuideCurve: {
            float padding = getRealConstant(GuideCurvePadding);
            VertexRange xRange = getVertexRange(mesh, Vertex::Phase);

            if (xRange.min >= padding * 0.75f && xRange.max <= 1.f - padding * 0.75f) {
                return;
            }

            remapVertexRange(mesh, Vertex::Phase, padding, 1.f - padding);
            break;
        }

        case LayerGroups::GroupWaveshaper: {
            float padding = getRealConstant(WaveshaperPadding);
            VertexRange xRange = getVertexRange(mesh, Vertex::Phase);
            VertexRange yRange = getVertexRange(mesh, Vertex::Amp);

            if (xRange.min >= padding * 0.75f && xRange.max <= 1.f - padding * 0.75f &&
                yRange.min >= padding * 0.75f && yRange.max <= 1.f - padding * 0.75f) {
                return;
            }

            remapVertexRange(mesh, Vertex::Phase, padding, 1.f - padding);
            remapVertexRange(mesh, Vertex::Amp, padding, 1.f - padding);
            break;
        }

        case LayerGroups::GroupIrModeller: {
            float padding = getRealConstant(IrModellerPadding);
            VertexRange xRange = getVertexRange(mesh, Vertex::Phase);

            if (xRange.min >= padding * 0.75f) {
                return;
            }

            remapVertexRange(mesh, Vertex::Phase, padding, 1.f);
            break;
        }

        default:
            return;
    }

    mesh->setVersion(Constants::MeshFormatVersion);
    mesh->validate();
}
