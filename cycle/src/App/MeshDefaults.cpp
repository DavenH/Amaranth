#include "MeshDefaults.h"

#include <JuceHeader.h>

#include <limits>

#include <Definitions.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>
#include <Curve/EnvelopeMesh.h>
#include <Curve/EnvRasterizer.h>
#include <Curve/Mesh.h>
#include <Curve/Vertex.h>
#include <Inter/EnvelopeInter2D.h>

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

        verts.push_back(new Vertex(padding * 0.5f,   0.5f));
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

    EnvRasterizer* getEnvelopeRasterizer(SingletonRepo* repo, int layerType) {
        switch (layerType) {
            case LayerGroups::GroupVolume:  return &repo->get<EnvRasterizer>("EnvVolumeRast");
            case LayerGroups::GroupPitch:   return &repo->get<EnvRasterizer>("EnvPitchRast");
            case LayerGroups::GroupScratch: return &repo->get<EnvRasterizer>("EnvScratchRast");
            default:                        return nullptr;
        }
    }

    void initEnvelopeMesh(SingletonRepo* repo, int layerType, EnvelopeMesh* mesh) {
        if (mesh == nullptr || mesh->getNumCubes() > 0 || mesh->getNumVerts() > 0) {
            return;
        }

        auto& interactor = repo->get<EnvelopeInter2D>("EnvelopeInter2D");
        EnvRasterizer* rast = getEnvelopeRasterizer(repo, layerType);

        if (rast == nullptr) {
            return;
        }

        int originalEnv = getSetting(CurrentEnvGroup);
        int originalLayerType = interactor.layerType;
        bool originalCollision = getSetting(CollisionDetection);
        bool originalSuspendUndo = interactor.suspendUndo;
        MeshRasterizer* originalRast = interactor.getRasterizer();

        rast->setMesh(mesh);
        rast->setMode(EnvRasterizer::NormalState);
        interactor.setRasterizer(rast);
        interactor.layerType = layerType;
        interactor.suspendUndo = true;

        rast->setCalcDepthDimensions(false);
        rast->update(Update);

        getSetting(CollisionDetection) = false;
        getSetting(CurrentEnvGroup) = layerType;

        switch (layerType) {
            case LayerGroups::GroupVolume:
                interactor.addNewCube(0.f, 0.f,    0.f,  1.f);
                interactor.addNewCube(0.f, 0.05f,  1.f,  0.5f);
                interactor.addNewCube(0.f, 0.7f,   0.8f, 0.3f);
                interactor.addNewCube(0.f, 0.999f, 0.8f, 1.f);
                mesh->setSustainToLast();
                interactor.addNewCube(0.f, 1.075f, 0.6f, -1.f);
                interactor.addNewCube(0.f, 1.15f,  0.f,  -1.f);
                interactor.addNewCube(0.f, 1.25f,  0.f,  -1.f);
                break;

            case LayerGroups::GroupPitch:
                interactor.addNewCube(0.f, 0.f,    0.5f, -1.f);
                interactor.addNewCube(0.f, 0.999f, 0.5f, -1.f);
                mesh->setSustainToLast();
                break;

            case LayerGroups::GroupScratch:
                interactor.addNewCube(0.f, 0.f,    0.f, 1.f);
                interactor.addNewCube(0.f, 0.999f, 1.f, 1.f);
                mesh->setSustainToLast();
                break;

            default:
                break;
        }

        interactor.resetState();
        interactor.suspendUndo = originalSuspendUndo;
        getSetting(CollisionDetection) = originalCollision;

        rast->update(Update);
        rast->setCalcDepthDimensions(true);

        getSetting(CurrentEnvGroup) = originalEnv;
        interactor.layerType = originalLayerType;

        if (originalRast != nullptr) {
            interactor.setRasterizer(originalRast);
        }
    }
}

void MeshDefaults::initialiseIfNeeded(SingletonRepo* repo, int layerType, Mesh* mesh) {
    // TODO(daven): This legacy Cycle mesh bootstrap is application-specific and
    // should move behind an explicit app extension hook instead of being invoked
    // ad hoc from startup/UI code.
    switch (layerType) {
        case LayerGroups::GroupVolume: initEnvelopeMesh(repo, layerType, dynamic_cast<EnvelopeMesh*>(mesh)); break;
        case LayerGroups::GroupPitch: initEnvelopeMesh(repo, layerType, dynamic_cast<EnvelopeMesh*>(mesh)); break;
        case LayerGroups::GroupScratch: initEnvelopeMesh(repo, layerType, dynamic_cast<EnvelopeMesh*>(mesh)); break;
        case LayerGroups::GroupGuideCurve: initGuideMesh(repo, mesh); break;
        case LayerGroups::GroupWaveshaper: initWaveshaperMesh(repo, mesh); break;
        case LayerGroups::GroupIrModeller: initTubeModelMesh(repo, mesh); break;
        default: break;
    }
}

void MeshDefaults::migrateLegacyPaddingIfNeeded(SingletonRepo* repo, int layerType, Mesh* mesh) {
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
