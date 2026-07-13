#pragma once

#include <App/SingletonAccessor.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

class GuideCurveProvider;

class EnvRasterizerService : public SingletonAccessor {
public:
    EnvRasterizerService(
            SingletonRepo* repo,
            GuideCurveProvider* guideCurveProvider,
            const String& name) :
            SingletonAccessor(repo, name)
        ,   rasterizer      (guideCurveProvider, name) {
    }

    EnvRasterizer& get() { return rasterizer; }
    const EnvRasterizer& get() const { return rasterizer; }
    void reset() override { rasterizer.reset(); }

private:
    EnvRasterizer rasterizer;
};

class EnvVolumeRast : public EnvRasterizerService {
public:
    using EnvRasterizerService::EnvRasterizerService;
};

class EnvPitchRast : public EnvRasterizerService {
public:
    using EnvRasterizerService::EnvRasterizerService;
};

class EnvScratchRast : public EnvRasterizerService {
public:
    using EnvRasterizerService::EnvRasterizerService;
};
