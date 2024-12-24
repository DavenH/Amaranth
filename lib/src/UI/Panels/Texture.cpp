#include <climits>
#include "Texture.h"
#include "ScopedGL.h"

using namespace gl;

TextureGL::TextureGL() :
        id			(UINT_MAX)
    , 	blendFunc	(GL_SRC_ALPHA) {
}

TextureGL::TextureGL(Image& image, int blendFunc) :
        Texture		(image)
    ,	id			(UINT_MAX)
    , 	blendFunc	(blendFunc) {
    TextureGL::create();
}

void TextureGL::bind() {
    if (image.isNull()) {
        return;
    }

    jassert(id != UINT_MAX);

    rect.setWidth(image.getWidth());
    rect.setHeight(image.getHeight());
    src = rect.withPosition(0, 0);

    glBindTexture(GL_TEXTURE_2D, id);

    Image::BitmapData pixelData(image, 0, 0,
                                image.getWidth(), image.getHeight(),
                                Image::BitmapData::readWrite);

    const void* pixels = pixelData.data;

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        rect.getWidth(),
        rect.getHeight(),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );
}

void TextureGL::create() {
    glGenTextures(1, &id);
}

void TextureGL::clear() {
    if (id != UINT_MAX) {
        glDeleteTextures(1, &id);
        id = UINT_MAX;
    }
}

void TextureGL::draw() {
    if (blendFunc != GL_SRC_ALPHA)
        glBlendFunc(blendFunc, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_LINE_SMOOTH);
//	glColor4f(1, 1, 1, 1);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    ScopedEnable tex2d(GL_TEXTURE_2D);

    {
        ScopedElement quads(GL_QUADS);

        float invH 	= 1.f / image.getHeight();
        float invW 	= 1.f / image.getWidth();

        float x1 	= src.getX() * invW;
        float y1 	= src.getY() * invH;
        float x2 	= src.getRight() * invW;
        float y2 	= src.getBottom() * invH;

        glTexCoord2f(x1, y1);
        glVertex2f(rect.getX(), rect.getY());

        glTexCoord2f(x2, y1);
        glVertex2f(rect.getRight(), rect.getY());

        glTexCoord2f(x2, y2);
        glVertex2f(rect.getRight(), rect.getBottom());

        glTexCoord2f(x1, y2);
        glVertex2f(rect.getX(), rect.getBottom());
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void TextureGL::drawSubImage(const Rectangle<float>& pos) {
    src = pos;
    draw();
}
