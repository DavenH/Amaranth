#pragma once
#include <vector>

#include "Panel2D.h"
#include "OpenGL.h"
#include "OpenGLBase.h"
#include "PanelOwner.h"
#include "../../App/SingletonAccessor.h"

using std::vector;

class Interactor;
class ZoomPanel;
class CommonGL;
class Panel2D;

class OpenGLPanel :
	 	public virtual SingletonAccessor
	,	public Component
	,	public OpenGLBase
	,	public OpenGLRenderer
	,	public Panel::Renderer
	,	public PanelOwner<Panel2D> {
public:
	OpenGLPanel(SingletonRepo* repo, Panel2D* panel);
	virtual ~OpenGLPanel();

	/* Opengl basics */
	void openGLContextClosing();
	void clear();
	void renderOpenGL();
	void newOpenGLContextCreated();
	void deactivateContext();
	void activateContext();
	void resized();
	void initRender();

	void activate() { activateContext(); }
	void deactivate() { deactivateContext(); }

protected:
	JUCE_LEAK_DETECTOR(OpenGLPanel)
};
