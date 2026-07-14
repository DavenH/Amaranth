#pragma once

#include <App/SingletonAccessor.h>
#include <Curve/Rasterization/Rasterizer/VoiceRasterizer.h>

class VoiceMeshRasterizer :
        public SingletonAccessor
    ,   public Rasterization::VoiceRasterizer {
public:
    explicit VoiceMeshRasterizer(SingletonRepo* repo);
    void reset() override { cleanUp(); }
};
