#include <stdio.h>
#include "CommonGL.h"
#include "OpenGLBase.h"
#include "Panel.h"
#include "ScopedGL.h"
#include "Texture.h"
#include "../../App/SingletonRepo.h"
#include "../../Inter/Interactor.h"
#include "../../Curve/Vertex2.h"

CommonGL::CommonGL(Panel* panel, OpenGLBase* parent) :
        CommonGfx(panel),
        parent(parent) {
}

void CommonGL::setCurrentColour(float float1, float float2, float float3, float float4) {
    glColor4f(float1, float2, float3, float4);
}

void CommonGL::setCurrentColour(float float1, float float2, float float3) {
    glColor3f(float1, float2, float3);
}

void CommonGL::setCurrentLineWidth(float width) {
    glLineWidth(width);
}

void CommonGL::setCurrentColour(const Color& color) {
    glColor4fv(color.v);
}

void CommonGL::disableSmoothing() {
    glDisable(GL_LINE_SMOOTH);
}

void CommonGL::enableSmoothing() {
    if (parent->smoothLines)
        glEnable(GL_LINE_SMOOTH);
}

void CommonGL::drawPoint(float size, Vertex2 point, bool scale) {
    glPointSize(size);

    ScopedElement glPoint(GL_POINTS);

    if (scale)
        glVertex2f(panel->sx(point.x), panel->sy(point.y));
    else
        glVertex2f(point.x, point.y);
}

void CommonGL::drawPoints(float pointSize, BufferXY& xy, bool scale) {
    glPointSize(pointSize);

    scaleIfNecessary(scale, xy);

    ScopedElement glPoint(GL_POINTS);

    for (int i = 0; i < xy.size(); ++i)
        glVertex2f(xy.x[i], xy.y[i]);
}

void CommonGL::drawLine(float x1, float y1, float x2, float y2,
                        const Color& c1, const Color& c2) {
    ScopedElement glLines(GL_LINES);

    glColor4fv(c1.v);
    glVertex2f(x1, y1);

    glColor4fv(c2.v);
    glVertex2f(x2, y2);
}

void CommonGL::drawPoints(float pointSize, BufferXY& xy, Buffer<float> c, bool scale) {
    int pixelStride = c.size() / xy.size();
    jassert(pixelStride == 3 || pixelStride == 4);

    scaleIfNecessary(scale, xy);

    glPointSize(pointSize);

    ScopedElement glPoint(GL_POINTS);

    if (pixelStride == 4) {
        for (int i = 0; i < (int) xy.size(); ++i) {
            glColor4fv(c.get() + 4 * i);
            glVertex2f(xy.x[i], xy.y[i]);
        }
    } else if (pixelStride == 3) {
        for (int i = 0; i < (int) xy.size(); ++i) {
            glColor3fv(c.get() + 3 * i);
            glVertex2f(xy.x[i], xy.y[i]);
        }
    }
}

void CommonGL::drawLine(float x1, float y1, float x2, float y2, bool scale) {
    scaleIfNecessary(scale, x1, y1, x2, y2);

    ScopedElement glLines(GL_LINES);

    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
}

void CommonGL::drawLineStrip(BufferXY& xy, bool singleCall, bool scale) {
    ScopedEnableClientState glState(GL_VERTEX_ARRAY, singleCall);

    int size = xy.size();
    panel->spliceBuffer.ensureSize(size * 2);

    scaleIfNecessary(scale, xy);
    xy.interlaceTo(panel->spliceBuffer);

    glVertexPointer((GLint) 2, GL_FLOAT, 0, panel->spliceBuffer);
    glDrawArrays(GL_LINE_STRIP, 0, size);
}

void CommonGL::initRender() {
    initLineParams();
}

void CommonGL::initLineParams() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1);

    if (parent->smoothLines)
        glEnable(GL_LINE_SMOOTH);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}


void CommonGL::drawRect(float leftX, float topY, float rightX, float botY, bool scale) {
    scaleIfNecessary(scale, leftX, topY, rightX, botY);

    ScopedElement quads(GL_LINE_LOOP);

    glVertex2f(leftX, topY);
    glVertex2f(rightX, topY);
    glVertex2f(rightX, botY);
    glVertex2f(leftX, botY);
}


void CommonGL::fillRect(float leftX, float topY, float rightX, float botY, bool scale) {
    scaleIfNecessary(scale, leftX, topY, rightX, botY);

    ScopedElement quads(GL_QUADS);

    glVertex2f(leftX, topY);
    glVertex2f(rightX, topY);
    glVertex2f(rightX, botY);
    glVertex2f(leftX, botY);
}


void CommonGL::fillAndOutlineColoured(const vector<ColorPos>& positions,
                                      float baseY, float baseAlpha,
                                      bool fill, bool outline) {
    enableSmoothing();

    if (fill) {
        ScopedElement quads(GL_QUAD_STRIP);

        for (int i = 0; i < (int) positions.size(); ++i) {
            const ColorPos& pos = positions[i];

            glColor4fv(pos.c.v);
            glVertex2f(pos.x, pos.y);

            glColor4fv(pos.c.withAlpha(baseAlpha).v);
            glVertex2f(pos.x, baseY);
        }
    }

    if (outline) {
        ScopedElement lineStrip(GL_LINE_STRIP);

        for (int i = 0; i < (int) positions.size(); ++i) {
            const ColorPos& pos = positions[i];

            glColor4fv(pos.c.withAlpha(1.f).v);
            glVertex2f(pos.x, pos.y);
        }
    }
}


void CommonGL::drawTexture(Texture* texture) {
    if (TextureGL* tex = dynamic_cast<TextureGL*>(texture))
        tex->draw();
}


void CommonGL::drawSubTexture(Texture* texture, const Rectangle<float>& pos) {
    if (TextureGL* tex = dynamic_cast<TextureGL*>(texture))
        tex->drawSubImage(pos);
}


void CommonGL::fillRect(float x1, float y1, float x2, float y2,
                        const Color& c1, const Color& c2) {
    ScopedElement quads(GL_QUADS);

    glColor4fv(c1.v);
    glVertex2f(x1, y1);

    glColor4fv(c2.v);
    glVertex2f(x1, y2);
    glVertex2f(x2, y2);

    glColor4fv(c1.v);
    glVertex2f(x2, y1);
}


void CommonGL::drawVerticalGradient(float left, float right, Buffer<float> y, const vector<Color>& colors) {
    ScopedElement quadStrip(GL_QUAD_STRIP);

    for (int i = 0; i < y.size(); ++i) {
        const Color& c = colors[i];

        glColor4fv(c.v);
        glVertex2f(left, y[i]);
        glVertex2f(right, y[i]);
    }
}


void CommonGL::checkErrors() {
    parent->printErrors(panel->getSingletonRepo());
}


void CommonGL::updateTexture(Texture* tex) {
    tex->bind();
}


void CommonGL::initializeTextures() {
    SingletonRepo* repo = panel->getSingletonRepo();
    dout << panel->panelName << " initializing textures\n";

    OwnedArray <Texture>& textures = panel->textures;
    TextureGL* nameA, * nameB, * grab, * scales, * dfrm;

    for (auto texture : textures)
        texture->clear();

    textures.clear();
    textures.add(nameA = new TextureGL(panel->nameImage, GL_ONE));
    textures.add(nameB = new TextureGL(panel->nameImageB, GL_ONE));
    textures.add(grab = new TextureGL(panel->grabImage));
    textures.add(scales = new TextureGL(panel->scalesImage));
    textures.add(dfrm = new TextureGL(panel->dfrmImage));

    grab->src = Rectangle<float>(0, 0, 24.f, 24.f);

    for (auto texture : textures)
        texture->bind();

    panel->grabTex = grab;
    panel->nameTexA = nameA;
    panel->nameTexB = nameB;
    panel->scalesTex = scales;
    panel->dfrmTex = dfrm;

    panel->updateNameTexturePos();
    panel->triggerPendingScaleUpdate();
}
