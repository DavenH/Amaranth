#pragma once
#include <vector>

#include "Panel2D.h"
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
	~OpenGLPanel() override;

	/* Opengl basics */
	void openGLContextClosing() override;
	void clear() override;
	void renderOpenGL() override;
	void newOpenGLContextCreated() override;
	void deactivateContext();
	void activateContext();
	void resized() override;
	void initRender();

	void activate() override { activateContext(); }
	void deactivate() override { deactivateContext(); }

protected:
	JUCE_LEAK_DETECTOR(OpenGLPanel)
};
