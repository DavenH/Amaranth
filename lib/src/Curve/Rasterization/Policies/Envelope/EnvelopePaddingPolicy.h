#pragma once

#include <vector>

#include "../../../Curve.h"
#include "../../../Intercept.h"

namespace Rasterization {
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
            for (int i = 0; i < (int) intercepts.size() - 2; ++i) {
                curves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
            }
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
}
