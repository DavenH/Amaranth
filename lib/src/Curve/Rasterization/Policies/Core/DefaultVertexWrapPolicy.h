#pragma once

namespace Rasterization {
    class DefaultVertexWrapPolicy {
    public:
        explicit DefaultVertexWrapPolicy(bool cyclic = true) :
                cyclic(cyclic) {
        }

        void wrap(float& ax, float& ay, float& bx, float& by, float independent) const {
            if (!cyclic) {
                return;
            }

            if (ay > 1.f && by > 1.f) {
                ay -= 1.f;
                by -= 1.f;
            } else if ((ay > 1.f) != (by > 1.f)) {
                float intercept = ax + (1.f - ay) / ((ay - by) / (ax - bx));
                if (intercept > independent) {
                    ay -= 1.f;
                    by -= 1.f;
                }
            }
        }

    private:
        bool cyclic;
    };
}
