#pragma once

#include "Texture.h"

class GLSurfaceCache {
public:
    void allocate(bool transparent);
    void captureFromFramebuffer(int componentHeight);
    void clear();
    void create();
    void draw() const;
    void setSize(int width, int height);

private:
    TextureGL texture;
};
