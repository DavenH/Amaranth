#include "GLSurfaceCache.h"

#include "ScopedGL.h"

using namespace gl;

void GLSurfaceCache::allocate(bool transparent) {
    this->transparent = transparent;

    const auto& rect = texture.rect;
    int format = transparent ? GL_RGBA : GL_RGB;

    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format,
        rect.getWidth(),
        rect.getHeight(),
        0,
        format,
        GL_UNSIGNED_BYTE,
        nullptr
    );
}

void GLSurfaceCache::captureFromFramebuffer(int componentHeight) {
    glBindTexture(GL_TEXTURE_2D, texture.id);

    const auto& rect = texture.rect;
    int width = rect.getWidth();
    int height = rect.getHeight();
    glCopyTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        0,
        componentHeight - height,
        width,
        height
    );

    juce::HeapBlock<juce::uint8> pixels(width * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, componentHeight - height, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Image::BitmapData bitmap(image, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < height; ++y) {
        const juce::uint8* srcRow = pixels.get() + (height - 1 - y) * width * 4;

        for (int x = 0; x < width; ++x) {
            const juce::uint8* src = srcRow + x * 4;
            juce::uint8 alpha = transparent ? src[3] : (juce::uint8) 255;
            bitmap.setPixelColour(x, y, juce::Colour::fromRGBA(src[0], src[1], src[2], alpha));
        }
    }

    {
        const juce::ScopedLock sl(snapshotLock);
        snapshot = image;
    }

    glClear(GL_COLOR_BUFFER_BIT);
}

void GLSurfaceCache::clear() {
    texture.clear();

    const juce::ScopedLock sl(snapshotLock);
    snapshot = {};
}

void GLSurfaceCache::create() {
    texture.create();
}

void GLSurfaceCache::draw() const {
    ScopedEnable tex2d(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, texture.id);
    glColor3f(1.f, 1.f, 1.f);

    const auto& rect = texture.rect;

    {
        ScopedElement glQuads(GL_QUADS);

        glTexCoord2f(0.f, 0.f);
        glVertex2f(0.f, rect.getHeight());

        glTexCoord2f(1.f, 0.f);
        glVertex2f(rect.getWidth(), rect.getHeight());

        glTexCoord2f(1.f, 1.f);
        glVertex2f(rect.getWidth(), 0.f);

        glTexCoord2f(0.f, 1.f);
        glVertex2f(0.f, 0.f);
    }
}

bool GLSurfaceCache::paintSnapshot(juce::Graphics& g, const juce::Rectangle<int>& bounds) const {
    const juce::ScopedLock sl(snapshotLock);

    if (!snapshot.isValid()) {
        return false;
    }

    g.drawImage(snapshot, bounds.toFloat());
    return true;
}

void GLSurfaceCache::setSize(int width, int height) {
    texture.rect.setSize(width, height);
}
