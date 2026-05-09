#pragma once

#include <App/AppConstants.h>
#include <App/MeshLibrary.h>
#include <Array/Buffer.h>
#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <Curve/EnvRasterizer.h>
#include <Util/Arithmetic.h>
#include <Util/NumberUtils.h>

#include "../../Audio/Effects/Unison.h"
#include "../../UI/VertexPanels/GuideCurvePanel.h"
#include "../../Util/CycleEnums.h"

namespace Cycle::Rasterization {
    class UnisonPhaseColumnRenderer {
    public:
        struct Context {
            MeshLibrary* meshLibrary {};
            EnvRasterizer* pitchRasterizer {};
            Unison* unison {};
            std::vector<Column>* columns {};

            int numFftOrders {};
            int currentMorphAxis { Vertex::Time };

            bool processUnison {};
            bool interpolate {};

            float lengthSeconds {};
            float time {};
            double panelPan { 0.5 };
        };

        void render(Context context) const {
            jassert(context.meshLibrary != nullptr);
            jassert(context.pitchRasterizer != nullptr);
            jassert(context.unison != nullptr);
            jassert(context.columns != nullptr);

            std::vector<Column>& columns = *context.columns;

            if (columns.size() < 2) {
                return;
            }

            int columnSize = 8 << (context.numFftOrders - 1);

            ScopedAlloc<Float32> memBuf(columnSize * 3);
            Buffer<float> columnBuf, phaseMoveBuffer, phaseMoveBuffer2;

            bool processUnison = context.processUnison && context.unison->isEnabled();

            Range<int> midiRange(Constants::LowestMidiNote, Constants::HighestMidiNote);
            int rawUnisonOrder = context.unison->getOrder(false);
            float unisonScale = powf(2.f, -(rawUnisonOrder - 1) * 0.14f);
            int unisonOrder = processUnison ? rawUnisonOrder : 1;

            ScopedAlloc<double> cumePhases(unisonOrder);
            cumePhases.zero();

            EnvRasterizer& pitch = *context.pitchRasterizer;

            // Calculates the curve for deferred sampleAt calls in processing.
            // Do this second to preserve guide curve contexts.
            pitch.updateOffsetSeeds(1, GuideCurvePanel::tableSize);
            pitch.ensureParamSize(unisonOrder);
            pitch.setCalcDepthDimensions(false);
            pitch.setWantOneSamplePerCycle(true);
            pitch.setMode(EnvRasterizer::NormalState);
            pitch.setLowresCurves(true);
            pitch.calcCrossPoints();
            pitch.setNoteOn();

            float timePerColEnv = 1.f / float(columns.size() - 1);
            float timePerColUni = timePerColEnv * context.lengthSeconds;

            int samplesPerCol = roundToInt(44100.f * timePerColEnv);
            double unitPortionPerSample = timePerColEnv / (float) samplesPerCol;

            for (auto& col : columns) {
                columnSize = col.size();
                float unitKey = Arithmetic::getUnitValueForGraphicNote(col.midiKey, midiRange);

                memBuf.resetPlacement();
                columnBuf        = memBuf.place(columnSize);
                phaseMoveBuffer  = memBuf.place(columnSize);
                phaseMoveBuffer2 = memBuf.place(columnSize);

                jassert(!(columnSize & (columnSize - 1)));

                col.copyTo(columnBuf);
                col.zero();

                for (int i = 0; i < unisonOrder; ++i) {
                    MeshLibrary::EnvProps* pitchProps =
                            context.meshLibrary->getCurrentEnvProps(LayerGroups::GroupPitch);

                    if (pitchProps == nullptr) {
                        continue;
                    }

                    if (pitchProps->active) {
                        pitch.renderToBuffer(samplesPerCol,
                                             unitPortionPerSample,
                                             EnvRasterizer::headUnisonIndex + i,
                                             *pitchProps,
                                             1.f);
                    }

                    float relativePan = 1.f, unisonCents = 0.f, uniPhase = 0.f;

                    if (processUnison) {
                        relativePan =
                                Arithmetic::getRelativePan(context.unison->getPan(i, false), context.panelPan)
                                * unisonScale;
                        unisonCents = context.unison->getDetune(i, false);
                        uniPhase    = context.unison->getPhase(i, false);
                    }

                    float envCents = getVoiceFrequencyCents(context, i);
                    float envHzAbove = Arithmetic::centsToFrequencyGraphic(unitKey, envCents, midiRange);
                    float uniHzAbove = Arithmetic::centsToFrequencyGraphic(unitKey, unisonCents, midiRange);
                    float phaseOffset = timePerColEnv * envHzAbove + timePerColUni * uniHzAbove;

                    double totalPhase = cumePhases[i] + uniPhase;
                    double scaledPhase = columnSize * (totalPhase + 10000);
                    long truncPhase = (long) scaledPhase;
                    double remainder = scaledPhase - truncPhase;

                    int phase = truncPhase & (columnSize - 1);

                    cumePhases[i] += phaseOffset;

                    jassert(remainder >= 0 && remainder <= 1);

                    if (context.interpolate) {
                        if (phase != 0 || remainder != 0) {
                            if (phase != 0) {
                                columnBuf.offset(phase).copyTo(phaseMoveBuffer);
                                columnBuf.copyTo(phaseMoveBuffer + (columnSize - phase));
                            } else {
                                columnBuf.copyTo(phaseMoveBuffer);
                            }

                            VecOps::mul(phaseMoveBuffer, 1.f - (float) remainder, phaseMoveBuffer2);
                            phaseMoveBuffer2.addProduct(phaseMoveBuffer + 1, remainder);
                            phaseMoveBuffer2[columnSize - 1] =
                                    (1 - remainder) * phaseMoveBuffer[columnSize - 1]
                                    + remainder * phaseMoveBuffer[0];

                            col.addProduct(phaseMoveBuffer2, relativePan);
                        } else {
                            col.addProduct(columnBuf, relativePan);
                        }
                    } else {
                        if (phase != 0) {
                            columnBuf.offset(phase).copyTo(phaseMoveBuffer);
                            columnBuf.copyTo(phaseMoveBuffer.offset(columnSize - (int) phase));

                            col.addProduct(phaseMoveBuffer, relativePan);
                        } else {
                            col.addProduct(columnBuf, relativePan);
                        }
                    }
                }
            }
        }

    private:
        static float getVoiceFrequencyCents(const Context& context, int unisonIndex) {
            double pitchEnvVal = 0.5;

            MeshLibrary::Properties* pitchProps =
                    context.meshLibrary->getCurrentProps(LayerGroups::GroupPitch);

            if (pitchProps != nullptr && pitchProps->active) {
                float y =
                        context.pitchRasterizer->getSustainLevel(EnvRasterizer::headUnisonIndex + unisonIndex);

                NumberUtils::constrain(y, 0.01f, 0.99f);
                pitchEnvVal = y;
            }

            return NumberUtils::unitPitchToSemis(pitchEnvVal) * 100;
        }
    };
}
