#pragma once

#include "MeshRasterizer.h"
#include "../App/MeshLibrary.h"

class SingletonRepo;

/**
 * Policies:
 *
 * - An env mesh's sustain line cannot be assigned to the last icpt
 * - If env mesh has a sustain line, this implies it has a release curve
 * - No sustain line means sustain on last icpt
 *
 */
class EnvRasterizer :
        public MeshRasterizer,
        public SingletonAccessor {
public:
    enum { loopMinSizeIcpts = 1, graphicIndex = 0, headUnisonIndex };
    enum { NormalState, Looping, Releasing };

    // one per unison voice
    class EnvParams {
    public:
        EnvParams() : sustainLevel(1.f),
                      samplePosition(0.),
                      sampleIndex(0) {
        }

        void reset() {
            samplePosition = 0;
            sampleIndex = 0;
            deformContext.currentIndex = 0;
        }

        int sampleIndex;
        float sustainLevel;
        double samplePosition;

        DeformContext deformContext;
    };

    /* ----------------------------------------------------------------------------- */

    explicit EnvRasterizer(SingletonRepo* repo, IDeformer* deformer, const String& name = String());
    EnvRasterizer& operator =(const EnvRasterizer& copy);
    EnvRasterizer(const EnvRasterizer& copy);
    ~EnvRasterizer() override;

    void calcCrossPoints() override;
    void ensureParamSize(int numUnisonVoices);
    void evaluateLoopSustainIndices();
    void getIndices(int& loopIdx, int& sustIdx) const;
    void padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) override;
    void processIntercepts(vector<Intercept>& intercepts) override;
    void resetGraphicParams();
    void setMesh(EnvelopeMesh* mesh);
    void setMesh(Mesh* mesh) override;
    void setNoteOff();
    void setNoteOn();
    void setWantOneSamplePerCycle(bool does);
    void simulateStart(double& lastPosition);
    void updateOffsetSeeds(int layerSize, int tableSize) override;
    void validateState();

    bool hasReleaseCurve();
    bool renderToBuffer(
            int numSamples,
            double deltaX,
            int unisonIdx, const
            MeshLibrary::EnvProps& props,
            float tempoScale);
    bool simulateRender(
            double deltaX,
            double& lastPosition,
            const MeshLibrary::EnvProps& props,
            float tempoScale);
    bool simulateStop(double& lastPosition);

    Mesh* getCurrentMesh();

    const EnvelopeMesh* getEnvMesh() const 		{ return envMesh; 							}
    float getSustainLevel(int paramIndex) const { return params[paramIndex].sustainLevel; 	}
    int getMode() const							{ return state; 							}
    void setMode(int mode)						{ this->state = mode; 						}
    bool wantsOneSamplePerCycle() const 		{ return oneSamplePerCycle; 				}
    Buffer<float> getRenderBuffer() 			{ return renderBuffer; 					}


private:
    void changedToRelease();
    void padIcptsForRender(vector<Intercept>& icpts, vector<Curve>& curves);

    bool canLoop() const;
    int vectorizedRenderToBuffer(Buffer<float> buffer, int numSamples, double deltaX, int unisonIdx);
    double getLoopLength() const;

    /* ----------------------------------------------------------------------------- */

    bool sampleReleaseNextCall, oneSamplePerCycle;
    int loopIndex, sustainIndex, state;
    double releaseScale;

    MeshLibrary::EnvProps defaultProps;
    EnvelopeMesh* envMesh;

    vector<EnvParams> params;

    ScopedAlloc<float> preallocated;
    ScopedAlloc<float> waveformMemory;
    Buffer<float> waveXCopy, waveYCopy, slopeCopy, renderBuffer;


    JUCE_LEAK_DETECTOR(EnvRasterizer)
};