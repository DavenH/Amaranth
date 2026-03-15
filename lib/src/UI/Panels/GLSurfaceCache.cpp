#include "GLSurfaceCache.h"

#include "ScopedGL.h"

using namespace gl;

void GLSurfaceCache::allocate(bool transparent) {
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
    glCopyTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        0,
        componentHeight - rect.getHeight(),
        rect.getWidth(),
        rect.getHeight()
    );

    glClear(GL_COLOR_BUFFER_BIT);
}

void GLSurfaceCache::clear() {
    texture.clear();
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

void GLSurfaceCache::setSize(int width, int height) {
    texture.rect.setSize(width, height);
}
