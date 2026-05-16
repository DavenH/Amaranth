#pragma once

#include <Design/Updating/Updateable.h>
#include "RasterizerViews.h"

namespace Rasterization {
    class Rasterizer : public Updateable {
    public:
        virtual ~Rasterizer() = default;

        virtual SamplerView sampler() const = 0;
        virtual SnapshotView snapshotView() = 0;

        virtual void updateGeometry() = 0;
        virtual void updateWaveform() = 0;

        virtual void performUpdate(UpdateType updateType) {
            if (updateType == Update) {
                updateWaveform();
            }
        }
    };
}
