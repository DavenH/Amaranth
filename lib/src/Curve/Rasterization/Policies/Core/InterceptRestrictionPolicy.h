#pragma once

#include <vector>

#include "../../../Intercept.h"
#include "../../../../Util/NumberUtils.h"

namespace Rasterization {
    class InterceptRestrictionPolicy {
    public:
        struct Context {
            bool cyclic;
            float minimumX;
            float maximumX;
            float minimumSeparation;

            Context() :
                    cyclic(false)
                ,   minimumX(0.f)
                ,   maximumX(1.f)
                ,   minimumSeparation(0.0001f) {
            }
        };

        explicit InterceptRestrictionPolicy(Context context = Context()) :
                context(context) {
        }

        void restrict(std::vector<Intercept>& intercepts) const {
            if (intercepts.empty()) {
                return;
            }

            clampAdjustedX(intercepts);
            enforceAdjustedXOrder(intercepts);
            copyAdjustedXToX(intercepts);
            enforceUpperBoundaryOrder(intercepts);
            enforceForwardOrder(intercepts);
        }

        const Context& getContext() const { return context; }

    private:
        void clampAdjustedX(std::vector<Intercept>& intercepts) const {
            for (auto& intercept : intercepts) {
                NumberUtils::constrain(intercept.adjustedX, context.minimumX, context.maximumX);
            }
        }

        void enforceAdjustedXOrder(std::vector<Intercept>& intercepts) const {
            for (int i = 1; i < (int) intercepts.size(); ++i) {
                Intercept& a = intercepts[i - 1];
                Intercept& b = intercepts[i];

                if (b.adjustedX < a.adjustedX && b.isWrapped == a.isWrapped) {
                    b.adjustedX = a.adjustedX + context.minimumSeparation;
                }
            }
        }

        static void copyAdjustedXToX(std::vector<Intercept>& intercepts) {
            for (auto& intercept : intercepts) {
                intercept.x = intercept.adjustedX;
            }
        }

        void enforceUpperBoundaryOrder(std::vector<Intercept>& intercepts) const {
            if (intercepts.back().x < context.maximumX) {
                return;
            }

            for (int i = (int) intercepts.size() - 1; i >= 1; --i) {
                Intercept& left = intercepts[i - 1];
                Intercept& right = intercepts[i];

                if (left.x >= right.x) {
                    left.x = right.x - context.minimumSeparation;
                }
            }
        }

        void enforceForwardOrder(std::vector<Intercept>& intercepts) const {
            for (int i = 1; i < (int) intercepts.size(); ++i) {
                Intercept& left = intercepts[i - 1];
                Intercept& right = intercepts[i];

                if (right.x <= left.x) {
                    right.x = left.x + context.minimumSeparation;
                }
            }
        }

        Context context;
    };
}
