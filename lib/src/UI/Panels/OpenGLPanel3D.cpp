#include "OpenGLPanel3D.h"

#include <Definitions.h>

#include "GLPanelRenderer.h"
#include "Panel3D.h"
#include "Panel.h"
#include "CommonGL.h"
#include "CursorHelper.h"
#include "../../UI/Panels/ScopedGL.h"

using namespace gl;

OpenGLPanel3D::OpenGLPanel3D(SingletonRepo* repo, Panel3D* panel3D, Panel3D::DataRetriever* retriever) :
        PanelOwner          (panel3D)
    ,   SingletonAccessor   (repo, panel3D->getName())
    ,   OpenGLBase          (this, this)
    ,   dataRetriever       (retriever)
{
}

OpenGLPanel3D::~OpenGLPanel3D() {
    detach();
}

void OpenGLPanel3D::init() {
    surfaceCache.setSize(1024, 1024);

    commonGL = new CommonGL(panel, this);
    panelRenderer = std::make_unique<GLPanelRenderer>(commonGL, &surfaceCache);

    panel->setComponent(this);
    panel->setUseVertices(true);
    panel->setGraphicsHelper(commonGL);
    panel->setPanelRenderer(panelRenderer.get());
    panel->setGraphicsRenderer(this);
    panel->setRenderHelper(this);

    attach();
}

void OpenGLPanel3D::drawCircle() {
    Interactor* interactor = panel->interactor;

    float radius = interactor->realValue(PencilRadius);
    const int numSegments = radius * 600;

    float cx = interactor->state.currentMouse.x;
    float cy = interactor->state.currentMouse.y;
    float r  = interactor->realValue(PencilRadius);

    float theta             = MathConstants<float>::twoPi / float(numSegments);
    float tangentialFactor  = tanf(theta);
    float radialFactor      = cosf(theta);

    float x = r;
    float y = 0;

    glColor3f(1, 1, 1);
    {
        ScopedElement glLoop(GL_LINE_LOOP);

        for (int i = 0; i < numSegments; ++i) {
            glVertex2f(panel->sx(x + cx), panel->sy(y + cy));
            float tx = -y;
            float ty = x;

            x += tx * tangentialFactor;
            y += ty * tangentialFactor;
            x *= radialFactor;
            y *= radialFactor;
        }
    }
}

void OpenGLPanel3D::enableClientArrays() {
    // enable vertex arrays
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
}

void OpenGLPanel3D::disableClientArrays() {
    // enable vertex arrays
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

void OpenGLPanel3D::drawSurfaceColumn(int x) {
    // before draw, specify vertex arrays
    glColorPointer(panel->draw.stride, GL_UNSIGNED_BYTE, 0, panel->colours);
    glVertexPointer((GLint) Panel3D::vertsPerQuad, GL_FLOAT, 0, panel->vertices);
    glDrawArrays(GL_QUAD_STRIP, 0, (panel->draw.sizeY + 1) * vertsPerPolygon);
}

// must be done for each relevant opengl context
void OpenGLPanel3D::populateExtensions() {
    String str((char*) glGetString(GL_EXTENSIONS));

    if (str.isEmpty()) {
        return;
    }

    extensions.addTokens(str, " ");
}

bool OpenGLPanel3D::isSupported(const String& extension) {
    return extensions.contains(extension, false);
}

void OpenGLPanel3D::clear() {
    clearAndOrtho(getWidth(), getHeight());
}

void OpenGLPanel3D::renderOpenGL() {
    panel->render();
    printErrors(repo);
}

void OpenGLPanel3D::newOpenGLContextCreated() {
    info(panel->getName() << " new context created\n");

    commonGL->initializeTextures();

    surfaceCache.create();
    surfaceCache.allocate(panel->isTransparent);

    commonGL->initLineParams();

    jassert(glGetError() == GL_NO_ERROR);
    auto safeThis = Component::SafePointer<OpenGLPanel3D>(this);
    MessageManager::callAsync([safeThis]() {
        if (safeThis != nullptr) {
            safeThis->repaint();
        }
    });
}

void OpenGLPanel3D::resized() {
    panel->panelResized();
}

void OpenGLPanel3D::deactivateContext() {
//  dout << panel->getName() << " detaching context\n";

    context.detach();
//
//   #ifdef SINGLE_OPENGL_THREAD
////    getObj(MasterRenderer).removeContext(this, panel->getName());
//   #endif
//  #else
//  makeCurrentContextInactive();
//  #endif
}

void OpenGLPanel3D::openGLContextClosing() {
    info(panel->getName() << " context closing, clearing textures \n");

    surfaceCache.clear();

    printErrors(repo);
}

void OpenGLPanel3D::initRender() {
    commonGL->initLineParams();
}

void OpenGLPanel3D::activateContext() {
//  dout << panel->getName() << " attaching context\n";
    attach();

//  getObj(MasterRenderer).addContext(this, panel->getName());
}
