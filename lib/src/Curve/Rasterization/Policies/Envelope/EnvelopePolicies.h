#pragma once

#include <vector>

#include <App/MeshLibrary.h>
#include <Definitions.h>

#include "../Curves/CurvePolicies.h"
#include "../../../Curve.h"
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/Intercept.h>

namespace Rasterization {
    struct EnvelopeMarkerResult {
        int loopIndex { -1 };
        int sustainIndex { -1 };
    };

    class EnvelopeMarkerPolicy {
    public:
        EnvelopeMarkerResult evaluate(
                const std::vector<Intercept>& intercepts,
                const EnvelopeMesh* mesh,
                int loopMinSizeIcpts) const {
            EnvelopeMarkerResult result;
            int end = (int) intercepts.size() - 1;

            if (intercepts.empty()) {
                return result;
            }

            if (mesh == nullptr) {
                result.sustainIndex = end;
                return result;
            }

            const std::set<VertCube*>& loopLines = mesh->loopCubes;
            const std::set<VertCube*>& sustLines = mesh->sustainCubes;

            for (int i = 0; i < (int) intercepts.size(); ++i) {
                VertCube* cube = intercepts[i].cube;

                if (cube == nullptr) {
                    continue;
                }

                if (loopLines.find(cube) != loopLines.end()) {
                    result.loopIndex = i;
                }

                if (sustLines.find(cube) != sustLines.end()) {
                    result.sustainIndex = i;
                }
            }

            if (result.sustainIndex < 0) {
                result.sustainIndex = end;
            }

            if (result.loopIndex >= 0
                && result.sustainIndex >= 0
                && result.sustainIndex - result.loopIndex < loopMinSizeIcpts) {
                result.loopIndex = -1;
            }

            return result;
        }
    };

    struct EnvelopePaddingContext {
        enum State {
            NormalState,
            Looping,
            Releasing
        };

        State state { NormalState };
        int loopMinSizeIcpts { 1 };
        int loopIndex { -1 };
        int sustainIndex { -1 };
        float loopLength { -1.f };
        bool canLoop {};
        bool hasReleaseCurve {};
    };

    class EnvelopePaddingPolicy {
    public:
        bool buildDisplayPadding(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves,
                const EnvelopePaddingContext& context) const {
            int end = (int) intercepts.size() - 1;

            if (end <= 0) {
                return false;
            }

            const Intercept& firstIcpt = intercepts[0];
            Intercept front1(-0.05f, firstIcpt.y);
            Intercept front2(-0.075f, firstIcpt.y);
            Intercept front3(-0.1f, firstIcpt.y);
            Intercept back1, back2, back3;

            const Intercept& endIcpt = intercepts[end];

            int numLoopPoints = context.sustainIndex - context.loopIndex;
            if (context.canLoop && !context.hasReleaseCurve && numLoopPoints >= 2) {
                back1 = Intercept(intercepts[context.loopIndex + 1]);
                back2 = Intercept(intercepts[context.loopIndex + 2]);

                back1.x += context.loopLength;
                back2.x += context.loopLength;
            } else {
                back1 = Intercept(endIcpt.x + 0.001f, endIcpt.y, 0, 0);
                back2 = Intercept(endIcpt.x + 0.002f, endIcpt.y, 0, 0);
            }

            back3 = Intercept(endIcpt.x + 0.003f, endIcpt.y, 0, 0);

            curves.clear();
            curves.reserve(intercepts.size() + 5);

            curves.emplace_back(front3, front2, front1);
            curves.emplace_back(front2, front1, intercepts[0]);
            curves.emplace_back(front1, intercepts[0], intercepts[1]);

            addInteriorCurves(intercepts, curves);

            curves.emplace_back(intercepts[end - 1], intercepts[end], back1);
            curves.emplace_back(intercepts[end], back1, back2);

            if (context.hasReleaseCurve || !context.canLoop) {
                curves.emplace_back(back1, back2, back3);
            }

            return true;
        }

        bool buildRenderPadding(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves,
                const EnvelopePaddingContext& context) const {
            int end = (int) intercepts.size() - 1;

            if (end <= 0) {
                return false;
            }

            Intercept loopBackIcptA, loopBackIcptB;

            int numLoopPoints = context.sustainIndex - context.loopIndex;
            bool loopable = context.state != EnvelopePaddingContext::Releasing
                         && context.canLoop
                         && numLoopPoints > 1;

            if (loopable) {
                jassert(context.loopIndex + context.loopMinSizeIcpts <= context.sustainIndex);

                loopBackIcptA = Intercept(intercepts[context.loopIndex + 1]);
                loopBackIcptA.x += context.loopLength;

                loopBackIcptB = Intercept(intercepts[context.loopIndex + 2]);
                loopBackIcptB.x += context.loopLength;
            }

            if (context.state == EnvelopePaddingContext::NormalState
                || context.state == EnvelopePaddingContext::Releasing) {
                buildNormalOrReleaseRenderPadding(
                        intercepts,
                        curves,
                        context,
                        loopable,
                        numLoopPoints,
                        loopBackIcptA,
                        loopBackIcptB);
            } else if (context.state == EnvelopePaddingContext::Looping && loopable) {
                buildLoopingRenderPadding(
                        intercepts,
                        curves,
                        context,
                        loopBackIcptA,
                        loopBackIcptB);
            } else {
                buildDisplayPadding(intercepts, curves, context);
            }

            return true;
        }

    private:
        static void addInteriorCurves(const std::vector<Intercept>& intercepts, std::vector<Curve>& curves) {
            CurveTripletPolicy::appendAdjacent(intercepts, curves);
        }

        static void buildNormalOrReleaseRenderPadding(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves,
                const EnvelopePaddingContext& context,
                bool loopable,
                int numLoopPoints,
                const Intercept& loopBackIcptA,
                const Intercept& loopBackIcptB) {
            int end = (int) intercepts.size() - 1;

            Intercept front1(-0.05f, intercepts[0].y);
            Intercept front2(-0.075f, intercepts[0].y);
            Intercept front3(-0.1f, intercepts[0].y);
            Intercept back1, back2, back3;

            const Intercept& endIcpt = intercepts[end];

            if (context.state == EnvelopePaddingContext::NormalState) {
                if (loopable) {
                    back1 = loopBackIcptA;
                    back2 = numLoopPoints > 1 ? loopBackIcptB : loopBackIcptA;
                } else {
                    back1 = Intercept(endIcpt.x + 0.001f, endIcpt.y);
                    back2 = Intercept(endIcpt.x + 0.002f, endIcpt.y);
                    back3 = Intercept(endIcpt.x + 2.5f, endIcpt.y);
                }
            } else {
                back1 = Intercept(endIcpt.x + 0.001f, endIcpt.y);
                back2 = Intercept(endIcpt.x + 0.002f, endIcpt.y);
                back3 = Intercept(endIcpt.x + 0.003f, endIcpt.y);
            }

            curves.clear();
            curves.reserve(intercepts.size() + 6);

            curves.emplace_back(front3, front2, front1);
            curves.emplace_back(front2, front1, intercepts[0]);
            curves.emplace_back(front1, intercepts[0], intercepts[1]);

            addInteriorCurves(intercepts, curves);

            curves.emplace_back(intercepts[end - 1], intercepts[end], back1);
            curves.emplace_back(intercepts[end], back1, back2);

            if (context.state == EnvelopePaddingContext::Releasing || !loopable) {
                curves.emplace_back(back1, back2, back3);
            }
        }

        static void buildLoopingRenderPadding(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves,
                const EnvelopePaddingContext& context,
                const Intercept& loopBackIcptA,
                const Intercept& loopBackIcptB) {
            std::vector<Intercept> loopIcpts;

            jassert(context.loopIndex >= 0);
            jassert(context.loopLength > 0);
            jassert(context.sustainIndex >= 2);

            loopIcpts.emplace_back(intercepts[context.sustainIndex - 2]);
            loopIcpts.emplace_back(intercepts[context.sustainIndex - 1]);

            loopIcpts[0].x -= context.loopLength;
            loopIcpts[1].x -= context.loopLength;

            for (int i = context.loopIndex; i <= context.sustainIndex; ++i) {
                loopIcpts.emplace_back(intercepts[i]);
            }

            loopIcpts.emplace_back(loopBackIcptA);
            loopIcpts.emplace_back(loopBackIcptB);

            curves.clear();
            curves.reserve(loopIcpts.size());

            addInteriorCurves(loopIcpts, curves);
        }
    };

    struct EnvelopePlaybackContext {
        int loopIndex { -1 };
        int sustainIndex { -1 };
        bool releasing {};
    };

    class EnvelopePlaybackPolicy {
    public:
        bool hasReleaseCurve(
                const std::vector<Intercept>& intercepts,
                int sustainIndex) const {
            if (intercepts.empty()) {
                return false;
            }

            return sustainIndex != (int) intercepts.size() - 1;
        }

        float loopLength(
                const std::vector<Intercept>& intercepts,
                const EnvelopePlaybackContext& context) const {
            if (context.loopIndex >= 0
                && context.loopIndex < (int) intercepts.size() - 1
                && context.sustainIndex >= 0
                && context.sustainIndex < (int) intercepts.size()) {
                return intercepts[context.sustainIndex].x - intercepts[context.loopIndex].x;
            }

            return -1.f;
        }

        double boundary(
                const std::vector<Intercept>& intercepts,
                const EnvelopePlaybackContext& context) const {
            if (intercepts.empty()) {
                return 0.;
            }

            if (context.releasing) {
                return intercepts.back().x;
            }

            if (context.sustainIndex >= 0 && context.sustainIndex < (int) intercepts.size()) {
                return intercepts[context.sustainIndex].x;
            }

            return intercepts.back().x;
        }
    };

    struct EnvelopeReleaseContext {
        bool bipolar {};
        int sustainIndex { -1 };
    };

    struct EnvelopeReleaseStart {
        int index { -1 };
        float position {};
        float scale { 1.f };
    };

    class EnvelopeReleasePolicy {
    public:
        int releaseIndex(const EnvelopeReleaseContext& context) const {
            return context.bipolar ? context.sustainIndex : context.sustainIndex + 1;
        }

        EnvelopeReleaseStart start(
                const std::vector<Intercept>& intercepts,
                const EnvelopeReleaseContext& context,
                float sustainLevel,
                float sampledReleaseLevel) const {
            EnvelopeReleaseStart result;
            result.index = releaseIndex(context);

            if (result.index < 0 || result.index >= (int) intercepts.size()) {
                return result;
            }

            result.position = intercepts[result.index].x;

            float initialReleaseLevel = sampledReleaseLevel < 0.5f ? 0.5f : sampledReleaseLevel;
            result.scale = sustainLevel / initialReleaseLevel;

            return result;
        }
    };

    struct EnvelopeRenderTimingContext {
        int numSamples {};
        double deltaX {};
        float tempoScale { 1.f };
        float loopLength { -1.f };
        const MeshLibrary::EnvProps* props {};
    };

    struct EnvelopeRenderTiming {
        double effectiveDelta {};
        int maxSamplesPerBuffer { 1 };
    };

    class EnvelopeRenderTimingPolicy {
    public:
        EnvelopeRenderTiming prepare(const EnvelopeRenderTimingContext& context) const {
            jassert(context.deltaX >= 0);
            jassert(context.props != nullptr);

            EnvelopeRenderTiming result;
            result.effectiveDelta = context.deltaX;

            if (context.props == nullptr) {
                return result;
            }

            if (context.props->tempoSync) {
                result.effectiveDelta /= context.tempoScale;
            }

            if (context.props->scale != 1) {
                result.effectiveDelta /= (double) context.props->getEffectiveScale();
            }

            if (result.effectiveDelta <= 0.) {
                return result;
            }

            double loopLength = jmax<double>(2. * result.effectiveDelta, context.loopLength);
            double maxIterativeAdvancement = 0.5;

            if (loopLength > 2. * result.effectiveDelta) {
                maxIterativeAdvancement = 0.9 * loopLength;
            }

            result.maxSamplesPerBuffer = jlimit(
                    1,
                    512,
                    int(maxIterativeAdvancement / result.effectiveDelta));

            return result;
        }
    };

    struct EnvelopeStateValidationContext {
        enum State {
            NormalState,
            Looping,
            Releasing
        };

        State state { NormalState };
        int headIndex {};
        int waveformSize {};
        double waveformEnd {};
        double loopStart {};
        double loopEnd {};
        double loopLength { -1. };
    };

    class EnvelopeStateValidationPolicy {
    public:
        template<typename Params>
        EnvelopeStateValidationContext::State validate(
                Params& params,
                const EnvelopeStateValidationContext& context) const {
            auto state = context.state;

            if (context.waveformSize <= 0) {
                state = EnvelopeStateValidationContext::NormalState;
            }

            clampToWaveform(params, context);

            if (state == EnvelopeStateValidationContext::Looping) {
                if (context.loopLength <= 0.) {
                    state = EnvelopeStateValidationContext::NormalState;
                } else {
                    wrapLoopingParams(params, context);
                }
            } else if (state == EnvelopeStateValidationContext::NormalState
                       && context.loopLength > 0.
                       && params.size() > context.headIndex
                       && isWithin(params[context.headIndex].samplePosition, context.loopStart, context.loopEnd)) {
                state = EnvelopeStateValidationContext::Looping;
            }

            return state;
        }

    private:
        template<typename Params>
        static void clampToWaveform(Params& params, const EnvelopeStateValidationContext& context) {
            for (auto& param : params) {
                if (context.waveformSize <= 0) {
                    param.samplePosition = 0.;
                    param.sampleIndex = 0;
                } else if (param.samplePosition > context.waveformEnd) {
                    param.samplePosition = context.waveformEnd;
                    param.sampleIndex = context.waveformSize - 1;
                }
            }
        }

        template<typename Params>
        static void wrapLoopingParams(Params& params, const EnvelopeStateValidationContext& context) {
            for (int i = context.headIndex; i < (int) params.size(); ++i) {
                double& position = params[i].samplePosition;

                if (position > context.loopEnd) {
                    position = jmax(context.loopStart, position - context.loopLength);
                } else if (position < context.loopStart) {
                    position = jmin(context.loopEnd, position + context.loopLength);
                }
            }
        }

        static bool isWithin(double value, double lower, double upper) {
            return value >= lower && value <= upper;
        }
    };

    struct EnvelopeSustainPointContext {
        int sustainIndex { -1 };
        bool addFloorPoint {};
    };

    class EnvelopeSustainPointPolicy {
    public:
        bool apply(std::vector<Intercept>& intercepts, const EnvelopeSustainPointContext& context) const {
            if (!context.addFloorPoint) {
                return false;
            }

            if (context.sustainIndex < 0 || context.sustainIndex >= (int) intercepts.size() - 1) {
                return false;
            }

            Intercept sustainIntercept = intercepts[context.sustainIndex];
            sustainIntercept.cube = nullptr;
            sustainIntercept.x += 0.0001f;

            if (sustainIntercept.y < 0.5f) {
                sustainIntercept.y = 0.5f;
            }

            sustainIntercept.shp = 1.f;
            intercepts.emplace_back(sustainIntercept);

            return true;
        }
    };
}
