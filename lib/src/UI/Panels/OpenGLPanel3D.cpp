#include "OpenGLPanel3D.h"

#include <Definitions.h>

#include "Panel3D.h"
#include "Panel.h"
#include "CommonGL.h"
#include "CursorHelper.h"
#include "../../UI/Panels/ScopedGL.h"

#define GL_BGRA 0x80E1

using namespace gl;

OpenGLPanel3D::OpenGLPanel3D(SingletonRepo* repo, Panel3D* panel3D, Panel3D::DataRetriever* retriever) :
		PanelOwner          (panel3D)
	,	SingletonAccessor	(repo, panel3D->getName())
	,	OpenGLBase			(this, this)
	,	dataRetriever		(retriever) {
	backTex.rect.setSize(1024, 1024);

	commonGL = new CommonGL(panel3D, this);

	panel3D->setComponent(this);
	panel3D->setUseVertices(true);
	panel3D->setGraphicsHelper(commonGL);
	panel3D->setGraphicsRenderer(this);
	panel3D->setRenderHelper(this);

	attach();
}

OpenGLPanel3D::~OpenGLPanel3D() {
    detach();
}

void OpenGLPanel3D::drawCurvesAndSurfaces() {
    ScopedEnable tex2d(GL_TEXTURE_2D);
	Rectangle<float>& r = backTex.rect;

	glBindTexture(GL_TEXTURE_2D, backTex.id);
	glColor3f(1, 1, 1);

	float x1 = 0.f;
	float x2 = 1.f;
	float y1 = 0.f;
	float y2 = 1.f;

	{
		ScopedElement glQuads(GL_QUADS);

		glTexCoord2f(x1, y1);
		glVertex2f(0.f, r.getHeight());

		glTexCoord2f(x2, y1);
		glVertex2f(r.getWidth(), r.getHeight());

		glTexCoord2f(x2, y2);
		glVertex2f(r.getWidth(), 0.f);

		glTexCoord2f(x1, y2);
		glVertex2f(0.f, 0.f);
	}
}

void OpenGLPanel3D::drawCircle() {
    Interactor* interactor = panel->interactor;

    float radius = interactor->realValue(PencilRadius);
	const int numSegments = radius * 600;

	float cx = interactor->state.currentMouse.x;
	float cy = interactor->state.currentMouse.y;
	float r  = interactor->realValue(PencilRadius);

	float theta 			= 2 * IPP_PI / float(numSegments);
	float tangentialFactor 	= tanf(theta);
	float radialFactor 		= cosf(theta);

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

    if (str.isEmpty())
        return;

    extensions.addTokens(str, " ");
}

bool OpenGLPanel3D::isSupported(const String& extension) {
    return extensions.contains(extension, false);
}

void OpenGLPanel3D::textureBakeFinished() {
    glBindTexture(GL_TEXTURE_2D, backTex.id);

	Rectangle<float>& r = backTex.rect;

	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0,
					    getHeight() - r.getHeight(),
					    r.getWidth(), r.getHeight());

	glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLPanel3D::clear() {
    clearAndOrtho(getWidth(), getHeight());
}

void OpenGLPanel3D::renderOpenGL() {
    panel->render();
    printErrors(repo);
}

void OpenGLPanel3D::newOpenGLContextCreated() {
    std::cout << panel->getName() << " new context created\n";

	commonGL->initializeTextures();

	backTex.create();

	Rectangle<float>& r = backTex.rect;
	int format = panel->isTransparent ? GL_RGBA : GL_RGB;

	glBindTexture(GL_TEXTURE_2D, backTex.id);
	glTexImage2D(GL_TEXTURE_2D, 0, format, r.getWidth(), r.getHeight(),
				 0, format, GL_UNSIGNED_BYTE, 0);

	commonGL->initLineParams();

	jassert(glGetError() == GL_NO_ERROR);
    repaint();
}

void OpenGLPanel3D::resized() {
    panel->panelResized();
}

void OpenGLPanel3D::deactivateContext() {
//	dout << panel->getName() << " detaching context\n";

    context.detach();
//
//   #ifdef SINGLE_OPENGL_THREAD
////	getObj(MasterRenderer).removeContext(this, panel->getName());
//   #endif
//  #else
//	makeCurrentContextInactive();
//  #endif
}

void OpenGLPanel3D::openGLContextClosing() {
    std::cout << panel->getName() << " context closing, clearing textures \n";
//	HGLRC cc = wglGetCurrentContext();

	/*
	for(int i = 0; i < panel->textures.size(); ++i)
		panel->textures[i]->clear();

	backTex.clear();
	*/

	printErrors(repo);
}

void OpenGLPanel3D::initRender() {
    commonGL->initLineParams();
}

void OpenGLPanel3D::activateContext() {
//	dout << panel->getName() << " attaching context\n";
    attach();

//	getObj(MasterRenderer).addContext(this, panel->getName());
}
