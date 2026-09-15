#pragma once

#include <array>

#include <App/AppConstants.h>
#include <App/MeshLibrary.h>
#include <Array/Buffer.h>
#include <Array/Column.h>
#include <Array/ScopedAlloc.h>
#include <Audio/CycleDsp/UnisonColumnMixer.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>
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
            pitch.updateOffsetSeeds(
                    1,
                    GuideCurvePanel::tableSize,
                    ::Rasterization::GuideCurveSeed::visualization(0x554e4953u));
            pitch.ensureParamSize(unisonOrder);
            pitch.setCalcDepthDimensions(false);
            pitch.setWantOneSamplePerCycle(true);
            pitch.setMode(EnvRasterizer::NormalState);
            pitch.setLowresCurves(true);
            pitch.updateWaveform();
            pitch.setNoteOn();

            float timePerColEnv = 1.f / float(columns.size() - 1);
            float timePerColUni = timePerColEnv * context.lengthSeconds;

            int samplesPerCol = roundToInt(44100.f * timePerColEnv);
            double unitPortionPerSample = timePerColEnv / (float) samplesPerCol;

            for (auto& col : columns) {
                columnSize = col.size();
                float unitKey = Arithmetic::getUnitValueForGraphicNote(col.midiKey, midiRange);
                std::array<float, CycleDsp::maximumUnisonOrder> phases {};
                std::array<float, CycleDsp::maximumUnisonOrder> gains {};

                memBuf.resetPlacement();
                columnBuf        = memBuf.place(columnSize);
                phaseMoveBuffer  = memBuf.place(columnSize);
                phaseMoveBuffer2 = memBuf.place(columnSize);

                jassert(!(columnSize & (columnSize - 1)));

                col.copyTo(columnBuf);

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
                    phases[(size_t) i] = (float) totalPhase;
                    gains[(size_t) i] = relativePan;
                    cumePhases[i] += phaseOffset;
                }
                CycleDsp::UnisonColumnMixer::mix(
                        columnBuf,
                        col,
                        phases.data(),
                        gains.data(),
                        unisonOrder,
                        phaseMoveBuffer,
                        phaseMoveBuffer2,
                        context.interpolate);
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
