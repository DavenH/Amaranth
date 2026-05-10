#pragma once

#include <memory>
#include <vector>

#include <Curve/Curve.h>
#include <Curve/Mesh.h>
#include <Curve/RasterizerData.h>
#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>
#include <Curve/Rasterization/Rasterizer.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Rasterization/RasterizationRequest.h>
#include <Curve/Rasterization/Policies/Curves/CurvePolicies.h>
#include <Curve/Rasterization/Policies/Curves/WaveformBakePolicy.h>
#include <Curve/Rasterization/RenderResult.h>
#include <Curve/Rasterization/Sampling/GuideCurveSampler.h>
#include <Curve/VertCube.h>
#include <Design/Updating/Updateable.h>
#include <Inter/Dimensions.h>
#include <Obj/ColorPoint.h>
#include <Obj/MorphPosition.h>

using std::vector;

class Interactor;
class GuideCurveProvider;

namespace Rasterization {
    class GuideCurveApplier;
    struct GuideCurvePolicyContext;
}

class MeshRasterizer :
        public Updateable
    ,   public Rasterization::Rasterizer {
public:
    enum ScalingType { Unipolar, Bipolar, HalfBipolar };

    using GuideCurveContext = Rasterization::GuideCurveContext;
    using GuideCurveRegion = Rasterization::GuideCurveRegion;

    struct RenderState {
        bool batchMode {};
        bool lowResCurves {};
        bool calcDepthDims {};
        int scalingType { 1 };
        MorphPosition pos;

        RenderState() = default;

        RenderState(
                bool batch,
                bool lowres,
                bool calcDepth,
                int scaling,
                const MorphPosition& pos) :
                batchMode(batch)
            ,   lowResCurves(lowres)
            ,   calcDepthDims(calcDepth)
            ,   scalingType(scaling)
            ,   pos(pos) {
        }
    };

    class ScopedRenderState {
    public:
        ScopedRenderState(MeshRasterizer* rasterizer, RenderState* state);
        ~ScopedRenderState();

    private:
        MeshRasterizer* rasterizer {};
        RenderState* state {};
    };

    typedef vector<GuideCurveRegion>::iterator GuideCurveIter;

    /* ----------------------------------------------------------------------------- */

    explicit MeshRasterizer(const String& name = String());
    MeshRasterizer(const MeshRasterizer& copy);
    MeshRasterizer& operator=(const MeshRasterizer& copy);
    ~MeshRasterizer() override;

    void applyGuideCurves(Intercept& icpt, const MorphPosition& morph, bool noOffsetAtEnds = false);
    void calcCrossPoints(Mesh* usedmesh, float oscPhase);
    void calcIntercepts();
    void initialise();
    void makeCopy();
    void restrictIntercepts(vector<Intercept>& intercepts);
    void updateValue(int dim, float value);

    void restoreStateFrom(RenderState& src);
    void saveStateTo(RenderState& src);
    Rasterization::RasterizationRequest createRasterizationRequest();

    bool isSampleable() const;
    bool isSampleableAt(float x) const;
    bool wasCleanedUp() const { return unsampleable; }

    Rasterization::SamplerView samplerView() const override { return Rasterization::SamplerView(createWaveformBuffers(), isSampleable()); }
    Rasterization::SnapshotView snapshotView() override { return Rasterization::SnapshotView(rastArrays); }

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
                createWaveformBuffers(),
                buffer,
                delta,
                phase);
    }

    /* ----------------------------------------------------------------------------- */

    void calcCrossPoints();
    void cleanUp();
    void padIcpts(vector<Intercept>& icpts, vector<Curve>& curves);
    void padIcptsWrapped(vector<Intercept>& intercepts, vector<Curve>& curves);
    void reset();
    void performUpdate(UpdateType updateType) override;
    void updateCurves();

    bool hasEnoughCubesForCrossSection();
    int getPrimaryViewDimension();

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
    int getPaddingSize() const override             { return paddingSize;               }
    int getOneIndex() const                         { return waveform.oneIndex;         }
    int getZeroIndex() const                        { return waveform.zeroIndex;        }

    const vector<Curve>& getCurves() const          { return curves;                    }
    const vector<Intercept>& getFrontIcpts() const  { return frontIcpts;                }
    const vector<Intercept>& getBackIcpts() const   { return backIcpts;                 }
    vector<ColorPoint>& getColorPoints()            { return colorPoints;               }
    RasterizerData& getRasterizerData()              { return rastArrays;                }
    const RasterizerData& getRasterizerData() const  { return rastArrays;                }
    GuideCurveProvider* getGuideCurveProvider() const override { return guideCurveProvider; }

    void setBatchMode(bool batch)                   { batchMode = batch;                }
    void setWrapsEnds(bool wraps)                   { cyclic = wraps;                   }
    void setCalcDepthDimensions(bool calc)          { calcDepthDims = calc;             }
    void setCalcInterceptsOnly(bool calc)           { calcInterceptsOnly = calc;        }
    void setDecoupleComponentDfrm(bool does)        { decoupleComponentDfrms = does;    }
    void setDims(const Dimensions& dims) override   { this->dims = dims;                }
    void setIntegralSampling(bool does)             { this->integralSampling = does;    }
    void setInterceptPadding(float value)           { interceptPadding = value;         }
    void setInterpolatesCurves(bool should)         { interpolateCurves = should;       }
    void setLimits(float min, float max)            { xMinimum = min; xMaximum = max;   }
    void setLowresCurves(bool areLow)               { lowResCurves  = areLow;           }
    void setNoiseSeed(int seed)                     { noiseSeed     = seed;             }
    void setOverridingDim(int dim)                  { overridingDim = dim;              }
    void setScalingMode(ScalingType type)           { scalingType   = type;             }
    void setToOverrideDim(bool does)                { overrideDim   = does;             }
    void setYellow(float yellow)                    { morph.time.setValueDirect(yellow);}
    void setBlue(float blue)                        { morph.blue.setValueDirect(blue);  }
    void setRed(float red)                          { morph.red.setValueDirect(red);    }
    void setMorphPosition(const MorphPosition& m)   { morph         = m;                }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { guideCurveProvider = provider; }

    Mesh* getMesh()                        { return mesh;                      }
    void setMesh(Mesh* mesh) override {
        this->mesh = mesh;
    }
    bool wrapsVertices() const override             { return cyclic;                    }
    void updateOffsetSeeds(int layerSize, int tableSize);
    float getInterceptPadding() const               { return interceptPadding;          }
    VertCube::ReductionData& getReductionData()     { return reduct;                    }

protected:
    void clearRasterizationResult(bool clearCurves);
    void markWaveformUnsampleable();
    void randomizeGuideCurveOffsetSeeds(int layerSize, int tableSize);
    bool canRasterizeMesh(Mesh* usedMesh) const;
    void beginCrossPointCalculation();
    void updateBuffers(int size);
    void calcWaveform();
    Rasterization::CurveResolutionPolicy::Context createCurveResolutionContext() const;
    Rasterization::WaveformBakePolicy::Context createWaveformBakeContext();
    void prepareCurvesForWaveform();
    const Rasterization::RenderResult& renderMeshSlice(Mesh* usedMesh, float oscPhase);
    void publishMeshSliceOutput(const Rasterization::RenderResult& output);
    void sortInterceptsIfNeeded();
    bool handleDegenerateInterceptOutput();
    void rebuildCurvesFromIntercepts();
    void finishCrossPointCalculation();
    Rasterization::WaveformBuffers createWaveformBuffers() const;
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
    Rasterization::TrilinearMeshSlicer meshSlicer;
    Rasterization::RenderResult meshSliceResult;

    Rasterization::GuideCurveOffsetSeeds guideCurveOffsetSeeds;

    float interceptPadding;
    float xMaximum, xMinimum;

    ScalingType scalingType;
    String name;

    Dimensions dims;
    MorphPosition morph;

    Rasterization::RenderResult result;
    RasterizerData rasterizerData;

    RasterizerData& rastArrays;
    vector<Intercept>& frontIcpts;
    vector<Intercept>& backIcpts;
    vector<Intercept>& icpts;
    vector<ColorPoint>& colorPoints;
    vector<Curve>& curves;
    vector<GuideCurveRegion>& guideCurveRegions;

    Rasterization::WaveformBuffers& waveform;
    VertCube::ReductionData reduct;
    GuideCurveProvider* guideCurveProvider;
    Mesh* mesh;

    JUCE_LEAK_DETECTOR(MeshRasterizer)
};
