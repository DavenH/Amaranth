#pragma once

#include <vector>
#include "Curve.h"
#include "Mesh.h"
#include "RasterizerData.h"
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
class IDeformer;

typedef vector<Intercept>::iterator IcptIter;
typedef vector<Intercept>::const_iterator ConstIcptIter;

class MeshRasterizer :
        public Updateable {
public:
    enum ScalingType { Unipolar, Bipolar, HalfBipolar };

    struct DeformContext {
        int phaseOffsetSeed;
        int vertOffsetSeed;
        int currentIndex;

        DeformContext() : phaseOffsetSeed(0), vertOffsetSeed(0), currentIndex(0) {}
    };

    struct DeformRegion {
        int deformChan;
        float amplitude;
        Intercept start, end;
    };

    struct RenderState {
        bool batchMode;
        bool lowResCurves;
        bool calcDepthDims;
        ScalingType scalingType;
        MorphPosition pos;

        RenderState() : batchMode(false), lowResCurves(false), calcDepthDims(false), scalingType(Bipolar) {}
        RenderState(bool batch, bool lowres, bool calcDepth, ScalingType scaling, const MorphPosition& pos) :
            batchMode(batch), lowResCurves(lowres), calcDepthDims(calcDepth), scalingType(scaling), pos(pos) {}
    };

    class ScopedRenderState {
    public:
        MeshRasterizer* rasterizer;
        RenderState* state;

        ScopedRenderState(MeshRasterizer* rast, RenderState* state) :
            rasterizer(rast), state(state) {
            rasterizer->saveStateTo(*state);
        }

        ~ScopedRenderState() {
            rasterizer->restoreStateFrom(*state);
        }
    };

    typedef vector<DeformRegion>::iterator DeformIter;

    /* ----------------------------------------------------------------------------- */

    explicit MeshRasterizer(const String& name = String());
    MeshRasterizer(const MeshRasterizer& copy);
    MeshRasterizer& operator=(const MeshRasterizer& copy);
    ~MeshRasterizer() override;

    void adjustDeformingSharpness();
    void applyDeformers(Intercept& icpt, const MorphPosition& morph, bool noOffsetAtEnds = false);
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

    bool isSampleable();
    bool isSampleableAt(float x);
    bool wasCleanedUp() const { return unsampleable; }

    float sampleAt(double angle);
    float sampleAt(double angle, int& currentIndex);
    float sampleAtDecoupled(double angle, DeformContext& context);
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
        float* dest = buffer.get();
        int size = buffer.size();

        if(waveX.empty()) {
            buffer.set(0.5f);
            return 0;
        }

        auto lastAngle = (float) (size * delta + phase);

        jassert(waveX.front() < phase && waveX.back() > lastAngle);

        if (waveX.front() > phase || waveX.back() < lastAngle) {
            buffer.zero();
            phase += delta * size;
        } else {
            int currentIndex = jmax(0, zeroIndex - 1);

            while(phase < waveX[currentIndex] && currentIndex > 0)
                currentIndex--;

            jassert(phase > waveX[currentIndex]);

            for(int i = 0; i < size; ++i) {
                while (phase >= waveX[currentIndex + 1]) {
                    currentIndex++;
                }

                dest[i] = ((float) phase - waveX[currentIndex]) * slope[currentIndex] + waveY[currentIndex];

                phase += delta;
            }
        }

        if(phase > 0.5) {
            phase -= 1;
        }

        return phase;
    }

    /* ----------------------------------------------------------------------------- */

    virtual void calcCrossPoints();
    virtual void calcCrossPointsAtTime(float x);
    virtual void cleanUp();
    virtual void handleOtherOverlappingLines(Vertex2 a, Vertex2 b, VertCube* cube);
    virtual void padIcpts(vector<Intercept>& icpts, vector<Curve>& curves);
    virtual void padIcptsWrapped(vector<Intercept>& intercepts, vector<Curve>& curves);
    virtual void preCleanup();
    virtual void processIntercepts(vector<Intercept>& intercepts) {}
    virtual void pullModPositionAndAdjust() {}
    virtual void reset();
    void performUpdate(UpdateType updateType) override;
    virtual void wrapVertices(float& ax, float& ay, float& bx, float& by, float indie);
    virtual void updateCurves();

    virtual bool hasEnoughCubesForCrossSection();
    virtual int getNumDims();
    virtual int getPrimaryViewDimension();
    virtual Mesh* getCrossPointsMesh();

    /* ----------------------------------------------------------------------------- */

    bool isBipolar() const                          { return scalingType == Bipolar || scalingType == HalfBipolar;  }
    bool doesIntegralSampling() const               { return integralSampling;          }
    bool doesCalcDepthDimensions() const            { return calcDepthDims;             }

    MorphPosition& getMorphPosition()               { return morph;                     }

    Buffer<float> getWaveX()                        { return waveX;                     }
    Buffer<float> getWaveY()                        { return waveY;                     }
    Buffer<float> getSlopes()                       { return slope;                     }
    Buffer<float> getDiffX()                        { return diffX;                     }

    const String& getName() const                   { return name;                      }
    int getPaddingSize() const                      { return paddingSize;               }
    int getOneIndex() const                         { return oneIndex;                  }
    int getZeroIndex() const                        { return zeroIndex;                 }

    const vector<Curve>& getCurves() const          { return curves;                    }
    const vector<Intercept>& getFrontIcpts() const  { return frontIcpts;                }
    const vector<Intercept>& getBackIcpts() const   { return backIcpts;                 }
    vector<ColorPoint>& getColorPoints()            { return colorPoints;               }
    RasterizerData& getRastData()                   { return rastArrays;                }
    IDeformer* getDeformer() const                  { return deformer;                  }

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
    void setNoiseSeed(int seed)                     { noiseSeed     = seed;             }
    void setOverridingDim(int dim)                  { overridingDim = dim;              }
    void setScalingMode(ScalingType type)           { scalingType   = type;             }
    void setToOverrideDim(bool does)                { overrideDim   = does;             }
    void setYellow(float yellow)                    { morph.time    = yellow;           }
    void setBlue(float blue)                        { morph.blue    = blue;             }
    virtual void setRed(float red)                  { morph.red     = red;              }
    void setMorphPosition(const MorphPosition& m)   { morph         = m;                }
    void setDeformer(IDeformer* panel)              { deformer      = panel;            }

    virtual Mesh* getMesh()                         { return mesh;                      }
    virtual void setMesh(Mesh* mesh)                { this->mesh = mesh;                }
    virtual bool wrapsVertices() const              { return cyclic;                    }
    virtual void updateOffsetSeeds(int layerSize, int tableSize);


protected:
    void updateBuffers(int size);
    void calcWaveform();
    void setResolutionIndices(float base);
    static void calcTransferTable();

    /* ----------------------------------------------------------------------------- */

    static float transferTable[Curve::resolution];

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
    int oneIndex;
    int overridingDim;
    int paddingSize;
    int zeroIndex;

    short phaseOffsetSeeds[128];
    short vertOffsetSeeds[128];

    float interceptPadding;
    float xMaximum, xMinimum;

    ScalingType scalingType;
    String name;

    Dimensions dims;
    MicroTimer timer;
    MorphPosition morph;
    RasterizerData rastArrays;

    vector<Intercept> frontIcpts, backIcpts, icpts;
    vector<ColorPoint> colorPoints;
    vector<Curve> curves;
    vector<DeformRegion> deformRegions;

    ScopedAlloc<float> memoryBuffer;
    ScopedAlloc<Int8u> alignedBytes;

    Buffer<float> waveX, waveY, diffX, slope, area;
    VertCube::ReductionData reduct;
    IDeformer* deformer;
    Mesh* mesh;

    JUCE_LEAK_DETECTOR(MeshRasterizer)
};
