#include "CommonGL.h"
#include "OpenGLPanel.h"
#include "Panel.h"
#include "Texture.h"

#include "../../App/SingletonRepo.h"

#include <Definitions.h>


OpenGLPanel::OpenGLPanel(SingletonRepo* repo, Panel2D* panel2D) :
        PanelOwner(panel2D)
    ,   SingletonAccessor(repo, "OpenGLPanel")
    ,   OpenGLBase(this, this) {
}

OpenGLPanel::~OpenGLPanel() {
    detach();
}

void OpenGLPanel::init() {
    commonGL = new CommonGL(panel, this);

    panel->setComponent(this);
    panel->setGraphicsHelper(commonGL);
    panel->setRenderHelper(this);
}

void OpenGLPanel::clear() {
    if (getWidth() == 0) {
        return;
    }

    clearAndOrtho(getWidth(), getHeight());
}

void OpenGLPanel::newOpenGLContextCreated() {
    info(panel->getName() << " new context created\n");

    commonGL->initializeTextures();
    commonGL->initLineParams();

    jassert(gl::glGetError() == gl::GL_NO_ERROR);

    repaint();
}

void OpenGLPanel::openGLContextClosing() {
    info(panel->getName() << " context closing, clearing textures\n");

    printErrors(repo);
}

void OpenGLPanel::renderOpenGL() {
    panel->render();
    printErrors(repo);
}

void OpenGLPanel::resized() {
    panel->panelResized();
}

void OpenGLPanel::activateContext()
{
//  dout << panel->getName() << " attaching context\n";

    attach();
}

void OpenGLPanel::deactivateContext()
{
//  dout << panel->getName() << " detaching context\n";

    context.detach();
}

void OpenGLPanel::initRender()
{
    commonGL->initLineParams();
}
