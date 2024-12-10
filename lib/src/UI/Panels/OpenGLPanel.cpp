#include "CommonGL.h"
#include "OpenGLPanel.h"
#include "Panel.h"
#include "ScopedGL.h"
#include "Texture.h"
#include "../MasterRenderer.h"
#include "../../App/SingletonRepo.h"


OpenGLPanel::OpenGLPanel(SingletonRepo* repo, Panel2D* panel2D) :
		PanelOwner(panel2D)
	,	SingletonAccessor(repo, "OpenGLPanel")
	,	OpenGLBase(this, this) {
	commonGL = new CommonGL(panel2D, this);

	panel2D->setComponent(this);
	panel2D->setGraphicsHelper(commonGL);
	panel2D->setRenderHelper(this);

	attach();
}

OpenGLPanel::~OpenGLPanel() {
    detach();
}

void OpenGLPanel::clear() {
    if (getWidth() == 0)
        return;

    clearAndOrtho(getWidth(), getHeight());
}

void OpenGLPanel::newOpenGLContextCreated() {
    dout << panel->getName() << " new context created\n";

    commonGL->initializeTextures();
    commonGL->initLineParams();

    jassert(glGetError() == GL_NO_ERROR);

    repaint();
}

void OpenGLPanel::openGLContextClosing() {
    dout << panel->getName() << " context closing, clearing textures\n";

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
//	dout << panel->getName() << " attaching context\n";

	attach();
}

void OpenGLPanel::deactivateContext()
{
//	dout << panel->getName() << " detaching context\n";

	context.detach();
}

void OpenGLPanel::initRender()
{
	commonGL->initLineParams();
}
