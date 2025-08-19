#pragma once

#include "Design/Updating/Updateable.h"
#include "../Interpolator/Interpolator.h"
#include "../Sampler/SamplerOutput.h"
#include "../State/RasterizerData.h"
#include "RasterizerParams.h"

class CurveGenerator;
class CurveSampler;
class PointPositioner;
class Mesh;
class CurvePiece;
struct Intercept;

#include <vector>

using std::unique_ptr;
using std::vector;

template<typename InputType, typename OutputPointType>
class Rasterizer : Updateable {
public:
    // DH: I'm not so sure about this DI pattern.
    // This is the full set of orchestrable units, maybe just should be in
    // a builder pattern so subsets can be chosen, with defaults, and this
    // builder could include config.
    // I dislike uniqueptrs... can these units just be move()'d or ctr-copy()'d?
    // We don't need to copy MeshRasterizers, so it'd only be 'slow' at init time, which is okay.
    // Should I have some static methods to create various MeshRasterizers?
    Rasterizer(
        unique_ptr<Interpolator<InputType, OutputPointType>> interpolator,
        unique_ptr<PointPositioner> positioners[],
        unique_ptr<CurveGenerator> generator,
        unique_ptr<CurveSampler> sampler
    )
        : interpolator(std::move(interpolator)),
          generator(std::move(generator)),
          sampler(std::move(sampler))
    {
        for (const auto& p : positioners) {
            this->positioners.emplace_back(p);
        }
    }

    void addPositioner(unique_ptr<PointPositioner> positioner) {
        positioners.emplace_back(std::move(positioner));
    }

    SamplerOutput runPipeline(InputType arg);


    void performUpdate(UpdateType updateType) override {
        // where are we gonna get the input type from - a pure virtual? Might be that we make this one pure virtual.
        runPipeline();
    }

    const RasterizerParams& getConfig() { return config; }

protected:
    unique_ptr<Interpolator<InputType, OutputPointType>> interpolator;
    unique_ptr<CurveGenerator> generator;
    unique_ptr<CurveSampler> sampler;
    vector<unique_ptr<PointPositioner> > positioners;

    RasterizerParams config;
};

