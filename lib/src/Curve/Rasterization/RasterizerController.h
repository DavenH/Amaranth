#pragma once

#include <functional>
#include <vector>

#include "RasterizerRuntime.h"
#include "../Curve.h"
#include "../Intercept.h"
#include "../Mesh.h"

namespace Rasterization {
    class RasterizerController {
    public:
        void setCleanupProvider(std::function<void(RasterizerRuntime)> provider) {
            cleanupProvider = provider;
        }

        void setCrossPointProvider(std::function<void()> provider) {
            crossPointProvider = provider;
        }

        void setUpdateCurvesProvider(std::function<void()> provider) {
            updateCurvesProvider = provider;
        }

        void setNumDimensionsProvider(std::function<int()> provider) {
            numDimensionsProvider = provider;
        }

        void setPaddingProvider(std::function<void(std::vector<Intercept>&, std::vector<Curve>&)> provider) {
            paddingProvider = provider;
        }

        void setProcessInterceptsProvider(std::function<void(std::vector<Intercept>&)> provider) {
            processInterceptsProvider = provider;
        }

        void setMeshAssignmentProvider(std::function<void(Mesh*)> provider) {
            meshAssignmentProvider = provider;
        }

        void setCrossSectionAvailabilityProvider(std::function<bool()> provider) {
            crossSectionAvailabilityProvider = provider;
        }

        void setPrimaryViewDimensionProvider(std::function<int()> provider) {
            primaryViewDimensionProvider = provider;
        }

        void setOffsetSeedsProvider(std::function<void(int, int)> provider) {
            offsetSeedsProvider = provider;
        }

        void resetProviders() {
            cleanupProvider = nullptr;
            crossPointProvider = nullptr;
            updateCurvesProvider = nullptr;
            numDimensionsProvider = nullptr;
            paddingProvider = nullptr;
            processInterceptsProvider = nullptr;
            meshAssignmentProvider = nullptr;
            crossSectionAvailabilityProvider = nullptr;
            primaryViewDimensionProvider = nullptr;
            offsetSeedsProvider = nullptr;
        }

        bool clean(RasterizerRuntime runtime) const {
            if (cleanupProvider == nullptr) {
                return false;
            }

            cleanupProvider(runtime);
            return true;
        }

        bool calcCrossPoints() const {
            if (crossPointProvider == nullptr) {
                return false;
            }

            crossPointProvider();
            return true;
        }

        bool updateCurves() const {
            if (updateCurvesProvider == nullptr) {
                return false;
            }

            updateCurvesProvider();
            return true;
        }

        bool pad(std::vector<Intercept>& intercepts, std::vector<Curve>& curves) const {
            if (paddingProvider == nullptr) {
                return false;
            }

            paddingProvider(intercepts, curves);
            return true;
        }

        bool processIntercepts(std::vector<Intercept>& intercepts) const {
            if (processInterceptsProvider == nullptr) {
                return false;
            }

            processInterceptsProvider(intercepts);
            return true;
        }

        void meshAssigned(Mesh* mesh) const {
            if (meshAssignmentProvider != nullptr) {
                meshAssignmentProvider(mesh);
            }
        }

        bool hasNumDimensionsProvider() const {
            return numDimensionsProvider != nullptr;
        }

        int numDimensions() const {
            return numDimensionsProvider();
        }

        bool hasCrossSectionAvailabilityProvider() const {
            return crossSectionAvailabilityProvider != nullptr;
        }

        bool hasEnoughCubesForCrossSection() const {
            return crossSectionAvailabilityProvider();
        }

        bool hasPrimaryViewDimensionProvider() const {
            return primaryViewDimensionProvider != nullptr;
        }

        int primaryViewDimension() const {
            return primaryViewDimensionProvider();
        }

        bool updateOffsetSeeds(int layerSize, int tableSize) const {
            if (offsetSeedsProvider == nullptr) {
                return false;
            }

            offsetSeedsProvider(layerSize, tableSize);
            return true;
        }

    private:
        std::function<void(RasterizerRuntime)> cleanupProvider;
        std::function<void()> crossPointProvider;
        std::function<void()> updateCurvesProvider;
        std::function<int()> numDimensionsProvider;
        std::function<void(std::vector<Intercept>&, std::vector<Curve>&)> paddingProvider;
        std::function<void(std::vector<Intercept>&)> processInterceptsProvider;
        std::function<void(Mesh*)> meshAssignmentProvider;
        std::function<bool()> crossSectionAvailabilityProvider;
        std::function<int()> primaryViewDimensionProvider;
        std::function<void(int, int)> offsetSeedsProvider;
    };
}
