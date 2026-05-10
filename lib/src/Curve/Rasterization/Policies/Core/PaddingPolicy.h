#pragma once

#include <vector>

#include "../Curves/CurvePolicies.h"
#include "../../../Curve.h"
#include "../../../Intercept.h"

namespace Rasterization {
    struct PaddingPolicyContext {
        float interceptPadding {};
        float minimumX {};
        float maximumX { 1.f };
        const std::vector<Intercept>* boundsIntercepts {};
    };

    class CyclicPaddingPolicy {
    public:
        explicit CyclicPaddingPolicy(PaddingPolicyContext context = PaddingPolicyContext()) :
                context(context) {
        }

        int build(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves,
                std::vector<Intercept>& frontPadding,
                std::vector<Intercept>& backPadding) const {
            int end = (int) intercepts.size() - 1;

            if (end < 1) {
                return 0;
            }

            frontPadding.clear();
            backPadding.clear();

            buildFrontPadding(intercepts, frontPadding);
            buildBackPadding(intercepts, backPadding);
            buildCurves(intercepts, curves, frontPadding, backPadding);

            return (int) frontPadding.size() - 3;
        }

    private:
        void buildFrontPadding(
                const std::vector<Intercept>& intercepts,
                std::vector<Intercept>& frontPadding) const {
            int end = (int) intercepts.size() - 1;
            float frontier = 0.f;
            float offset = -1.f;
            int idx = end;
            int remainingIters = 2;

            frontPadding.emplace_back(intercepts[1]);
            frontPadding.emplace_back(intercepts[0]);

            for (;;) {
                if (remainingIters <= 0) {
                    break;
                }

                if (frontier < -context.interceptPadding) {
                    --remainingIters;
                }

                frontier = intercepts[idx].x + offset;
                Intercept padIntercept = intercepts[idx];
                padIntercept.x += offset;

                frontPadding.emplace_back(padIntercept);
                --idx;

                if (idx < 0) {
                    idx = end;
                    offset -= 1.f;
                }
            }
        }

        void buildBackPadding(
                const std::vector<Intercept>& intercepts,
                std::vector<Intercept>& backPadding) const {
            int end = (int) intercepts.size() - 1;
            float frontier = 1.f;
            float offset = 1.f;
            int idx = 0;
            int remainingIters = 2;

            backPadding.emplace_back(intercepts[end - 1]);
            backPadding.emplace_back(intercepts[end]);

            for (;;) {
                if (remainingIters <= 0) {
                    break;
                }

                if (frontier > 1.f + context.interceptPadding) {
                    --remainingIters;
                }

                frontier = intercepts[idx].x + offset;
                Intercept padIntercept = intercepts[idx];
                padIntercept.x += offset;

                backPadding.emplace_back(padIntercept);
                ++idx;

                if (idx > end) {
                    idx = 0;
                    offset += 1.f;
                }
            }
        }

        static void buildCurves(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves,
                const std::vector<Intercept>& frontPadding,
                const std::vector<Intercept>& backPadding) {
            curves.clear();
            curves.reserve(intercepts.size() + frontPadding.size() - 2 + backPadding.size() - 2);

            CurveTripletPolicy::appendAdjacentReversed(frontPadding, curves);
            CurveTripletPolicy::appendAdjacent(intercepts, curves);
            CurveTripletPolicy::appendAdjacent(backPadding, curves);
        }

        PaddingPolicyContext context;
    };

    class NonCyclicPaddingPolicy {
    public:
        explicit NonCyclicPaddingPolicy(PaddingPolicyContext context = PaddingPolicyContext()) :
                context(context) {
        }

        int build(const std::vector<Intercept>& intercepts, std::vector<Curve>& curves) const {
            int end = (int) intercepts.size() - 1;

            if (end <= 0) {
                return 2;
            }

            float minX = 1.f;
            float maxX = -1.f;
            const std::vector<Intercept>* boundsIntercepts = context.boundsIntercepts != nullptr
                                                           ? context.boundsIntercepts
                                                           : &intercepts;

            for (auto& intercept : *boundsIntercepts) {
                float x = intercept.x;
                maxX = x > maxX ? x : maxX;
                minX = x < minX ? x : minX;
            }

            minX = jmin(context.minimumX - 0.25f, minX);
            maxX = jmax(context.maximumX + 0.25f, maxX);

            Intercept front1(minX - 0.08f, intercepts[0].y);
            Intercept front2(minX - 0.16f, intercepts[0].y);
            Intercept front3(minX - 0.24f, intercepts[0].y);

            Intercept back1(maxX + 0.08f, intercepts[end].y);
            Intercept back2(maxX + 0.16f, intercepts[end].y);
            Intercept back3(maxX + 0.24f, intercepts[end].y);

            curves.clear();
            curves.reserve(intercepts.size() + 6);
            curves.emplace_back(front3, front2, front1);
            curves.emplace_back(front2, front1, intercepts[0]);
            curves.emplace_back(front1, intercepts[0], intercepts[1]);

            jassert(intercepts[0].y == intercepts[0].y);
            CurveTripletPolicy::appendAdjacent(intercepts, curves);

            curves.emplace_back(intercepts[end - 1], intercepts[end], back1);
            curves.emplace_back(intercepts[end], back1, back2);
            curves.emplace_back(back1, back2, back3);

            return 2;
        }

    private:
        PaddingPolicyContext context;
    };

    class FxPaddingPolicy {
    public:
        int build(const std::vector<Intercept>& intercepts, std::vector<Curve>& curves) const {
            int end = (int) intercepts.size() - 1;

            if (end <= 0) {
                return 1;
            }

            Intercept front1(-1.0f, intercepts[0].y);
            Intercept front2(-0.5f, intercepts[0].y);

            Intercept back1(1.5f, intercepts[end].y);
            Intercept back2(2.0f, intercepts[end].y);

            for (auto& curve : curves) {
                curve.destruct();
            }

            curves.clear();
            curves.reserve(intercepts.size() + 2);
            curves.emplace_back(front1, front2, intercepts[0]);
            curves.emplace_back(front2, intercepts[0], intercepts[1]);

            CurveTripletPolicy::appendAdjacent(intercepts, curves);

            curves.emplace_back(intercepts[end - 1], intercepts[end], back1);
            curves.emplace_back(intercepts[end], back1, back2);

            for (auto& curve : curves) {
                curve.construct();
            }

            return 1;
        }
    };
}
