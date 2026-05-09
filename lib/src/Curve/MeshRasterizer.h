#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "Curve.h"
#include "Mesh.h"
#include "RasterizerData.h"
#include "Rasterization/MeshRasterizerState.h"
#include "Rasterization/GuideCurveOffsetSeeds.h"
#include "Rasterization/RasterizationRequest.h"
#include "Rasterization/RasterizerRuntime.h"
#include "Rasterization/Policies/CurveResolutionPolicy.h"
#include "Rasterization/Policies/WaveformBakePolicy.h"
#include "Rasterization/Pipelines/MeshSlicePipeline.h"
#include "Rasterization/Sampling/GuideCurveSampler.h"
#include "SurfaceLine.h"
#include "VertCube.h"
#include "../Array/ScopedAlloc.h"
#include "../Design/Updating/Updateable.h"
#include "../Inter/Dimensions.h"
#include "../Obj/ColorPoint.h"
#include "../Obj/MorphPosition.h"
#include "../Util/MicroTimer.h"

using std::vector;

class Interactor;
class GuideCurveProvider;

namespace Rasterization {
    class GuideCurveApplier;
    struct GuideCurvePolicyContext;
    class MeshRasterizerFacade;
}

typedef vector<Intercept>::iterator IcptIter;
typedef vector<Intercept>::const_iterator ConstIcptIter;

class MeshRasterizer :
        public Updateable {
public:
    enum ScalingType { Unipolar, Bipolar, HalfBipolar };

    using GuideCurveContext = Rasterization::GuideCurveContext;
    using GuideCurveRegion = Rasterization::GuideCurveRegion;

    using RenderState = Rasterization::MeshRasterizerRenderState;
    using ScopedRenderState = Rasterization::ScopedMeshRasterizerRenderState;

    typedef vector<GuideCurveRegion>::iterator GuideCurveIter;

    /* ----------------------------------------------------------------------------- */

    explicit MeshRasterizer(const String& name = String());
    MeshRasterizer(const MeshRasterizer& copy);
    MeshRasterizer& operator=(const MeshRasterizer& copy);
    ~MeshRasterizer() override;

    void adjustDeformingSharpness();
    void applyGuideCurves(Intercept& icpt, const MorphPosition& morph, bool noOffsetAtEnds = false);
    void calcCrossPoints(Mesh* usedmesh, float oscPhase);
    void calcIntercepts();
    void calcWaveformFrom(vector<Intercept>& icpts);
    void initialise();
    void makeCopy();
    void oversamplingChanged();
    void print(OutputStream& stream);
    void restrictIntercepts(vector<Intercept>& intercepts);
    void separateIntercepts(vector<Intercept>& intercepts, float minDx);
    void updateValue(int dim, float value);
    void validateCurves();

    void restoreStateFrom(RenderState& src);
    void saveStateTo(RenderState& src);
    Rasterization::RasterizationRequest createRasterizationRequest();
    Rasterization::RasterizerRuntime createRasterizerRuntime();

    bool isSampleable();
    bool isSampleableAt(float x);
    bool wasCleanedUp() const { return unsampleable; }

    float sampleAt(double angle);
    float sampleAt(double angle, int& currentIndex);
    float sampleAtDecoupled(double angle, GuideCurveContext& context);
    float samplePerfectly(double delta, Buffer<float> buffer, double phase);
    void sampleAtIntervals(Buffer<float> deltas, Buffer<float> dest);

    /* ----------------------------------------------------------------------------- */

    void sampleEvenlyTo(const Buffer<float>& dest)
    {
        if(dest.empty()) {
            return;
        }

        sampleWithInterval<float>(dest, 1.f / float(dest.size() - 1), 0);
    }

    template<typename T>
    T sampleWithInterval(Buffer<float> buffer, T delta, T phase) {
        return Rasterization::WaveformSampler::sampleWithInterval(
                createWaveformView(),
                buffer,
                delta,
                phase);
    }

    /* ----------------------------------------------------------------------------- */

    virtual void calcCrossPoints();
    void calcCrossPointsAtTime(float x);
    void cleanUp();
    void handleOtherOverlappingLines(Vertex2 a, Vertex2 b, VertCube* cube);
    virtual void padIcpts(vector<Intercept>& icpts, vector<Curve>& curves);
    virtual void padIcptsWrapped(vector<Intercept>& intercepts, vector<Curve>& curves);
    void preCleanup();
    virtual void processIntercepts(vector<Intercept>& intercepts) {}
    void reset();
    void performUpdate(UpdateType updateType) override;
    void wrapVertices(float& ax, float& ay, float& bx, float& by, float indie);
    void updateCurves();

    bool hasEnoughCubesForCrossSection();
    int getNumDims();
    int getPrimaryViewDimension();
    Mesh* getCrossPointsMesh();

    /* ----------------------------------------------------------------------------- */

    bool isBipolar() const                          { return scalingType == Bipolar || scalingType == HalfBipolar;  }
    bool doesIntegralSampling() const               { return integralSampling;          }
    bool doesCalcDepthDimensions() const            { return calcDepthDims;             }
    ScalingType getScalingType() const              { return scalingType;               }

    MorphPosition& getMorphPosition()               { return morph;                     }

    Buffer<float> getWaveX()                        { return waveform.waveX;            }
    Buffer<float> getWaveY()                        { return waveform.waveY;            }
    Buffer<float> getSlopes()                       { return waveform.slope;            }
    Buffer<float> getDiffX()                        { return waveform.diffX;            }

    const String& getName() const                   { return name;                      }
    int getPaddingSize() const                      { return paddingSize;               }
    int getOneIndex() const                         { return waveform.oneIndex;         }
    int getZeroIndex() const                        { return waveform.zeroIndex;        }

    const vector<Curve>& getCurves() const          { return curves;                    }
    const vector<Intercept>& getFrontIcpts() const  { return frontIcpts;                }
    const vector<Intercept>& getBackIcpts() const   { return backIcpts;                 }
    vector<ColorPoint>& getColorPoints()            { return colorPoints;               }
    RasterizerData& getRastData()                   { return rastArrays;                }
    GuideCurveProvider* getGuideCurveProvider() const  { return guideCurveProvider;        }

    void setBatchMode(bool batch)                   { batchMode = batch;                }
    void setWrapsEnds(bool wraps)                   { cyclic = wraps;                   }
    void setCalcDepthDimensions(bool calc)          { calcDepthDims = calc;             }
    void setCalcInterceptsOnly(bool calc)           { calcInterceptsOnly = calc;        }
    void setDecoupleComponentDfrm(bool does)        { decoupleComponentDfrms = does;    }
    void setDims(const Dimensions& dims)            { this->dims = dims;                }
    void setIntegralSampling(bool does)             { this->integralSampling = does;    }
    void setInterceptPadding(float value)           { interceptPadding = value;         }
    void setInterpolatesCurves(bool should)         { interpolateCurves = should;       }
    void setLimits(float min, float max)            { xMinimum = min; xMaximum = max;   }
    void setLowresCurves(bool areLow)               { lowResCurves  = areLow;           }
    void setCleanupProvider(std::function<void(Rasterization::RasterizerRuntime)> provider) {
        cleanupProvider = provider;
    }
    void setCrossPointProvider(std::function<void()> provider) {
        crossPointProvider = provider;
    }
    void setUpdateCurvesProvider(std::function<void()> provider) {
        updateCurvesProvider = provider;
    }
    void setNoiseSeed(int seed)                     { noiseSeed     = seed;             }
    void setNumDimensionsProvider(std::function<int()> provider) {
        numDimensionsProvider = provider;
    }
    void setOverridingDim(int dim)                  { overridingDim = dim;              }
    void setPaddingProvider(std::function<void(vector<Intercept>&, vector<Curve>&)> provider) {
        paddingProvider = provider;
    }
    void setMeshAssignmentProvider(std::function<void(Mesh*)> provider) {
        meshAssignmentProvider = provider;
    }
    void setCrossSectionAvailabilityProvider(std::function<bool()> provider) {
        crossSectionAvailabilityProvider = provider;
    }
    void setPrimaryViewDimensionProvider(std::function<int()> provider) {
        primaryViewDimensionProvider = provider;
    }
    void setScalingMode(ScalingType type)           { scalingType   = type;             }
    void setToOverrideDim(bool does)                { overrideDim   = does;             }
    void setYellow(float yellow)                    { morph.time.setValueDirect(yellow);}
    void setBlue(float blue)                        { morph.blue.setValueDirect(blue);  }
    void setRed(float red)                          { morph.red.setValueDirect(red);    }
    void setMorphPosition(const MorphPosition& m)   { morph         = m;                }
    void setGuideCurveProvider(GuideCurveProvider* provider) { guideCurveProvider = provider; }

    virtual Mesh* getMesh()                         { return mesh;                      }
    virtual void setMesh(Mesh* mesh) {
        this->mesh = mesh;

        if (meshAssignmentProvider != nullptr) {
            meshAssignmentProvider(mesh);
        }
    }
    bool wrapsVertices() const                      { return cyclic;                    }
    virtual void updateOffsetSeeds(int layerSize, int tableSize);


protected:
    void markWaveformUnsampleable();
    bool canRasterizeMesh(Mesh* usedMesh) const;
    void beginCrossPointCalculation();
    void updateBuffers(int size);
    void calcWaveform();
    Rasterization::CurveResolutionPolicy::Context createCurveResolutionContext() const;
    Rasterization::WaveformBakePolicy::Context createWaveformBakeContext();
    void prepareCurvesForWaveform();
    Rasterization::MeshSlicePipeline::Output renderMeshSlice(Mesh* usedMesh, float oscPhase);
    void publishMeshSliceOutput(const Rasterization::MeshSlicePipeline::Output& output);
    void sortInterceptsIfNeeded();
    bool handleDegenerateInterceptOutput();
    void rebuildCurvesFromIntercepts();
    void finishCrossPointCalculation();
    Rasterization::WaveformBuffers createWaveformView() const;
    Rasterization::WaveformBufferRefs createWaveformRefs();
    Rasterization::GuideCurvePolicyContext createGuideCurvePolicyContext();
    Rasterization::GuideCurveApplier createGuideCurveApplier();
    void setResolutionIndices(float base);

    /* ----------------------------------------------------------------------------- */

    // flags

    bool batchMode;
    bool calcDepthDims;
    bool calcInterceptsOnly;
    bool cyclic;
    bool decoupleComponentDfrms;
    bool integralSampling;
    bool interpolateCurves;
    bool lowResCurves;
    bool needsResorting;
    bool overrideDim;
    bool unsampleable;

    int noiseSeed;
    int overridingDim;
    int paddingSize;
    std::function<void()> crossPointProvider;
    std::function<void()> updateCurvesProvider;
    std::function<void(Rasterization::RasterizerRuntime)> cleanupProvider;
    std::function<void(vector<Intercept>&, vector<Curve>&)> paddingProvider;
    std::function<void(Mesh*)> meshAssignmentProvider;
    std::function<int()> numDimensionsProvider;
    std::function<bool()> crossSectionAvailabilityProvider;
    std::function<int()> primaryViewDimensionProvider;

    Rasterization::GuideCurveOffsetSeeds guideCurveOffsetSeeds;

    float interceptPadding;
    float xMaximum, xMinimum;

    ScalingType scalingType;
    String name;

    Dimensions dims;
    MicroTimer timer;
    MorphPosition morph;
    RasterizerData rastArrays;
    std::unique_ptr<Rasterization::MeshRasterizerFacade> facade;

    vector<Intercept> frontIcpts, backIcpts, icpts;
    vector<ColorPoint> colorPoints;
    vector<Curve> curves;
    vector<GuideCurveRegion> guideCurveRegions;

    ScopedAlloc<float> memoryBuffer;
    ScopedAlloc<Int8u> alignedBytes;

    Rasterization::WaveformBuffers waveform;
    VertCube::ReductionData reduct;
    GuideCurveProvider* guideCurveProvider;
    Mesh* mesh;

    JUCE_LEAK_DETECTOR(MeshRasterizer)
};
