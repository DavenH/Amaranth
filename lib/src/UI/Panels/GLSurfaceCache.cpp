#include <climits>

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

void GLSurfaceCache::captureFromFramebuffer(
    const juce::Rectangle<int>& componentBounds,
    juce::Point<int> framebufferOriginPixels
) {
    glBindTexture(GL_TEXTURE_2D, texture.id);

    int width = jmin(activePixelBounds.getWidth(), roundToInt(componentBounds.getWidth() * renderScale));
    int height = jmin(activePixelBounds.getHeight(), roundToInt(componentBounds.getHeight() * renderScale));

    if (width <= 0 || height <= 0) {
        return;
    }

    int sourceX = framebufferOriginPixels.x + roundToInt(componentBounds.getX() * renderScale);
    int sourceY = framebufferOriginPixels.y + roundToInt(componentBounds.getBottom() * renderScale) - height;
    sourceY = jmax(0, sourceY);

    glCopyTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        sourceX,
        sourceY,
        width,
        height
    );

    juce::HeapBlock<juce::uint8> pixels(width * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(sourceX, sourceY, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

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
}

void GLSurfaceCache::clear() {
    texture.clear();
    activeBounds = {};
    activePixelBounds = {};

    const juce::ScopedLock sl(snapshotLock);
    snapshot = {};
}

void GLSurfaceCache::create() {
    texture.create();
}

void GLSurfaceCache::draw() const {
    if (texture.id == UINT_MAX) {
        return;
    }

    int activeWidth = activeBounds.getWidth();
    int activeHeight = activeBounds.getHeight();
    int activePixelWidth = activePixelBounds.getWidth();
    int activePixelHeight = activePixelBounds.getHeight();
    int backingWidth = (int) texture.rect.getWidth();
    int backingHeight = (int) texture.rect.getHeight();

    if (activeWidth <= 0 || activeHeight <= 0 ||
        activePixelWidth <= 0 || activePixelHeight <= 0 ||
        backingWidth <= 0 || backingHeight <= 0) {
        return;
    }

    ScopedEnable tex2d(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, texture.id);
    glColor3f(1.f, 1.f, 1.f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    float u2 = activePixelWidth / (float) backingWidth;
    float v2 = activePixelHeight / (float) backingHeight;

    {
        ScopedElement glQuads(GL_QUADS);

        glTexCoord2f(0.f, 0.f);
        glVertex2f(0.f, (float) activeHeight);

        glTexCoord2f(u2, 0.f);
        glVertex2f((float) activeWidth, (float) activeHeight);

        glTexCoord2f(u2, v2);
        glVertex2f((float) activeWidth, 0.f);

        glTexCoord2f(0.f, v2);
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
    activeBounds.setSize(width, height);
    updatePixelBounds();
}

void GLSurfaceCache::setRenderScale(double scale) {
    renderScale = jmax(1.0, scale);
    updatePixelBounds();

    if (texture.id != UINT_MAX &&
        (texture.rect.getWidth() != activePixelBounds.getWidth() ||
         texture.rect.getHeight() != activePixelBounds.getHeight())) {
        texture.rect.setSize(activePixelBounds.getWidth(), activePixelBounds.getHeight());
        allocate(transparent);
    }
}

void GLSurfaceCache::updatePixelBounds() {
    activePixelBounds.setSize(
        jmax(1, roundToInt(activeBounds.getWidth() * renderScale)),
        jmax(1, roundToInt(activeBounds.getHeight() * renderScale))
    );

    if (texture.rect.getWidth() <= 0 || texture.rect.getHeight() <= 0) {
        texture.rect.setSize(activePixelBounds.getWidth(), activePixelBounds.getHeight());
    }
}
