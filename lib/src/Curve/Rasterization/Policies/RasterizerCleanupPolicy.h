#pragma once

#include "../RasterizerRuntime.h"

namespace Rasterization {
    struct RasterizerCleanupOptions {
        bool clearIntercepts { true };
        bool clearCurves { true };
        bool clearFrontPadding { true };
        bool clearBackPadding { true };
        bool clearColorPoints { true };
    };

    class RasterizerCleanupPolicy {
    public:
        explicit RasterizerCleanupPolicy(RasterizerCleanupOptions options = {}) :
                options(options) {
        }

        void clean(RasterizerRuntime runtime) const {
            jassert(runtime.isBound());

            markWaveformUnsampleable(runtime);

            if (options.clearIntercepts && runtime.intercepts != nullptr) {
                runtime.intercepts->clear();
            }

            if (options.clearCurves && runtime.curves != nullptr) {
                runtime.curves->clear();
            }

            if (options.clearFrontPadding && runtime.frontPadding != nullptr) {
                runtime.frontPadding->clear();
            }

            if (options.clearBackPadding && runtime.backPadding != nullptr) {
                runtime.backPadding->clear();
            }

            if (options.clearColorPoints && runtime.colorPoints != nullptr) {
                runtime.colorPoints->clear();
            }
        }

        static void markWaveformUnsampleable(RasterizerRuntime runtime) {
            jassert(runtime.waveform.waveX != nullptr);
            jassert(runtime.waveform.waveY != nullptr);
            jassert(runtime.unsampleable != nullptr);

            runtime.waveform.waveX->nullify();
            runtime.waveform.waveY->nullify();
            *runtime.unsampleable = true;
        }

    private:
        RasterizerCleanupOptions options;
    };
}
