#pragma once

#include <Curve/VertCube.h>
#include <Curve/Vertex.h>

namespace Cycle::Rasterization {
    class VoicePointPositionPolicy {
    public:
        struct Context {
            float voiceTime {};
            float oscPhase {};
        };

        struct Result {
            bool intersects {};
            float phase {};
        };

        Result resolve(
                const Context& context,
                bool pointOverlaps,
                Vertex* a,
                Vertex* b,
                Vertex* vertex) const {
            if (!pointOverlaps) {
                return {};
            }

            jassert(a != nullptr && b != nullptr && vertex != nullptr);
            jassert(a->values[Vertex::Phase] >= 0.f);

            normalizeOverlapEndpoints(context.voiceTime, a, b);
            VertCube::vertexAt(context.voiceTime, Vertex::Time, a, b, vertex);

            return { true, wrapPhase(vertex->values[Vertex::Phase] + context.oscPhase) };
        }

    private:
        static void normalizeOverlapEndpoints(float voiceTime, Vertex* a, Vertex* b) {
            if (a->values[Vertex::Phase] > 1.f && b->values[Vertex::Phase] > 1.f) {
                a->values[Vertex::Phase] -= 1.f;
                b->values[Vertex::Phase] -= 1.f;
            }

            if ((a->values[Vertex::Phase] > 1.f) != (b->values[Vertex::Phase] > 1.f)) {
                float interceptTime = a->values[Vertex::Time]
                                    + (1.f - a->values[Vertex::Phase])
                                      / ((a->values[Vertex::Phase] - b->values[Vertex::Phase])
                                         / (a->values[Vertex::Time] - b->values[Vertex::Time]));

                if (interceptTime > voiceTime) {
                    a->values[Vertex::Phase] -= 1.f;
                    b->values[Vertex::Phase] -= 1.f;
                }
            }
        }

        static float wrapPhase(float phase) {
            while (phase >= 1.f) {
                phase -= 1.f;
            }

            while (phase < 0.f) {
                phase += 1.f;
            }

            jassert(phase >= 0.f && phase < 1.f);
            return phase;
        }
    };
}
