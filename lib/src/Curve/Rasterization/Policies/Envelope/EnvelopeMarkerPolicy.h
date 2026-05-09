#pragma once

#include <vector>

#include "../../../EnvelopeMesh.h"
#include "../../../Intercept.h"

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
}
