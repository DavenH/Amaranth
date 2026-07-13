#include "MeshDefaults.h"

#include <JuceHeader.h>

#include <limits>

#include <Definitions.h>
#include <App/AppConstants.h>
#include <App/SingletonRepo.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Inter/EnvelopeInter2D.h>

#include "EnvRasterizerServices.h"
#include "../Util/CycleEnums.h"

namespace {
    constexpr float legacyGuideCurvePadding = 0.05f;
    constexpr float legacyIrModellerGeometryPadding = 0.0625f;
    constexpr float legacyWaveshaperGeometryPadding = 0.125f;

    String layerGroupName(int layerType) {
        switch (layerType) {
            case LayerGroups::GroupVolume: return "Volume";
            case LayerGroups::GroupPitch: return "Pitch";
            case LayerGroups::GroupScratch: return "Scratch";
            case LayerGroups::GroupGuideCurve: return "GuideCurve";
            case LayerGroups::GroupWaveshaper: return "Waveshaper";
            case LayerGroups::GroupIrModeller: return "IrModeller";
            default: return "Unknown(" + String(layerType) + ")";
        }
    }

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

    String describeMeshGeometry(Mesh* mesh) {
        if (mesh == nullptr) {
            return "mesh=null";
        }

        StringArray pieces;
        pieces.add("mesh=" + String::toHexString((pointer_sized_int) mesh));
        pieces.add("version=" + String(mesh->getVersion()));
        pieces.add("verts=" + String(mesh->getNumVerts()));
        pieces.add("cubes=" + String(mesh->getNumCubes()));

        if (mesh->getNumVerts() > 0) {
            VertexRange phaseRange = getVertexRange(mesh, Vertex::Phase);
            VertexRange ampRange = getVertexRange(mesh, Vertex::Amp);
            VertexRange timeRange = getVertexRange(mesh, Vertex::Time);

            pieces.add(String::formatted("phase=[%.4f, %.4f]", phaseRange.min, phaseRange.max));
            pieces.add(String::formatted("amp=[%.4f, %.4f]", ampRange.min, ampRange.max));
            pieces.add(String::formatted("time=[%.4f, %.4f]", timeRange.min, timeRange.max));

            StringArray verts;
            int index = 0;

            for (auto* vert : mesh->getVerts()) {
                verts.add(String::formatted("#%d(t=%.4f p=%.4f a=%.4f r=%.4f b=%.4f c=%.4f)",
                                            index++,
                                            vert->values[Vertex::Time],
                                            vert->values[Vertex::Phase],
                                            vert->values[Vertex::Amp],
                                            vert->values[Vertex::Red],
                                            vert->values[Vertex::Blue],
                                            vert->values[Vertex::Curve]));
            }

            pieces.add("vertices={" + verts.joinIntoString(", ") + "}");
        }

        return pieces.joinIntoString(" ");
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

    bool rangeAlreadyUsesEffectPadding(const VertexRange& range, float padding, bool hasRightPadding) {
        float leftTolerance = padding * 0.375f;
        float expectedMin = padding * 0.5f;

        if (range.min < expectedMin - leftTolerance) {
            return false;
        }

        if (!hasRightPadding) {
            return true;
        }

        float expectedMax = 1.f - padding * 0.5f;
        return range.max <= expectedMax + leftTolerance;
    }

    void initTubeModelMesh(SingletonRepo* repo, Mesh* mesh) {
        if (mesh == nullptr || mesh->getNumVerts() > 0) {
            return;
        }

        float padding = legacyIrModellerGeometryPadding;
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

        float padding = legacyWaveshaperGeometryPadding;
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

        float padding = legacyGuideCurvePadding;
        auto& verts = mesh->getVerts();

        verts.push_back(new Vertex(padding,       0.5f));
        verts.push_back(new Vertex(1.f - padding, 0.5f));

        verts[0]->setMaxSharpness();
        verts[1]->setMaxSharpness();
    }

    EnvRasterizer* getEnvelopeRasterizer(SingletonRepo* repo, int layerType) {
        switch (layerType) {
            case LayerGroups::GroupVolume:  return &repo->get<EnvVolumeRast>("EnvVolumeRast").get();
            case LayerGroups::GroupPitch:   return &repo->get<EnvPitchRast>("EnvPitchRast").get();
            case LayerGroups::GroupScratch: return &repo->get<EnvScratchRast>("EnvScratchRast").get();
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
        EnvRasterizer* originalRast = interactor.getRast(originalEnv);

        rast->setMesh(mesh);
        rast->setDims(interactor.dims);
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

        interactor.setRasterizer(originalRast);
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
        // DBG("MeshDefaults::migrateLegacyPaddingIfNeeded skip group=" + layerGroupName(layerType)
        //     + " reason=empty " + describeMeshGeometry(mesh));
        return;
    }

    // DBG("MeshDefaults::migrateLegacyPaddingIfNeeded before group=" + layerGroupName(layerType)
    //     + " " + describeMeshGeometry(mesh));

    switch (layerType) {
        case LayerGroups::GroupGuideCurve: {
            float padding = legacyGuideCurvePadding;
            VertexRange xRange = getVertexRange(mesh, Vertex::Phase);

            if (xRange.min >= padding * 0.75f && xRange.max <= 1.f - padding * 0.75f) {
                // DBG("MeshDefaults::migrateLegacyPaddingIfNeeded skip group=" + layerGroupName(layerType)
                //     + " reason=already-padded padding=" + String(padding));
                return;
            }

            remapVertexRange(mesh, Vertex::Phase, padding, 1.f - padding);
            break;
        }

        case LayerGroups::GroupWaveshaper: {
            float padding = legacyWaveshaperGeometryPadding;
            VertexRange xRange = getVertexRange(mesh, Vertex::Phase);
            VertexRange yRange = getVertexRange(mesh, Vertex::Amp);

            if (rangeAlreadyUsesEffectPadding(xRange, padding, true) &&
                rangeAlreadyUsesEffectPadding(yRange, padding, true)) {
                // DBG("MeshDefaults::migrateLegacyPaddingIfNeeded skip group=" + layerGroupName(layerType)
                //     + " reason=already-padded padding=" + String(padding));
                return;
            }

            remapVertexRange(mesh, Vertex::Phase, padding, 1.f - padding);
            remapVertexRange(mesh, Vertex::Amp, padding, 1.f - padding);
            break;
        }

        case LayerGroups::GroupIrModeller: {
            float padding = legacyIrModellerGeometryPadding;
            VertexRange xRange = getVertexRange(mesh, Vertex::Phase);

            if (rangeAlreadyUsesEffectPadding(xRange, padding, false)) {
                // DBG("MeshDefaults::migrateLegacyPaddingIfNeeded skip group=" + layerGroupName(layerType)
                //     + " reason=already-padded padding=" + String(padding));
                return;
            }

            remapVertexRange(mesh, Vertex::Phase, padding, 1.f);
            break;
        }

        default:
            // DBG("MeshDefaults::migrateLegacyPaddingIfNeeded skip group=" + layerGroupName(layerType)
            //     + " reason=unsupported");
            return;
    }

    mesh->setVersion(Constants::MeshFormatVersion);
    mesh->validate();

    // DBG("MeshDefaults::migrateLegacyPaddingIfNeeded after group=" + layerGroupName(layerType)
    //     + " " + describeMeshGeometry(mesh));
}
