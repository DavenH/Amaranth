#pragma once

#include <vector>

#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"

namespace CycleV2 {

class TrimeshRenderProfile;

struct TrimeshRenderData {
    std::vector<float> surface;
    std::vector<float> linearFrequencySurface;
    std::vector<float> slice;
    PortDomain domain { PortDomain::TimeSignal };
    int rows {};
    int columns {};
    bool cyclic { true };

    bool canDrawSurface() const {
        return rows >= 2 && columns >= 2 && surface.size() >= (size_t) rows * (size_t) columns;
    }
};

class TrimeshGridRenderService {
public:
    static TrimeshRenderData renderGrid(
            TrimeshNodeModel& model,
            int rows,
            int columns,
            PortDomain domain = PortDomain::TimeSignal,
            int midiNote = 48);
    static TrimeshRenderData renderGrid(
            TrimeshNodeModel& model,
            int rows,
            int columns,
            const TrimeshRenderProfile& renderProfile,
            int midiNote = 48);
};

}
