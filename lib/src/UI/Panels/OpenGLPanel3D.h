#pragma once
#include "Panel3D.h"
#include "OpenGL.h"
#include "OpenGLBase.h"
#include "PanelOwner.h"
#include "Texture.h"
#include "../../App/SingletonAccessor.h"

using std::vector;

class CommonGL;
class Interactor3D;

class OpenGLPanel3D :
		public OpenGLBase
	,	public PanelOwner<Panel3D>
	,	public Panel3D::Renderer
	, 	public OpenGLRenderer
	,	public Component
	, 	public virtual SingletonAccessor {
public:
	OpenGLPanel3D(SingletonRepo* repo, Panel3D* panel3D, Panel3D::DataRetriever* retriever);
	virtual ~OpenGLPanel3D();

	void drawSurfaceColumn(int x);
	void drawCurvesAndSurfaces();
	void resized();
	void drawCircle();
	void initRender();

	// opengl crap
	void activateContext();
	void deactivateContext();

	void disableClientArrays();
	void enableClientArrays();
	void newOpenGLContextCreated();
	void openGLContextClosing();
	void renderOpenGL();
	void textureBakeFinished();

	// panel3d renderer
	void clear();
	void activate() { activateContext(); }
	void deactivate() { deactivateContext(); }

	CriticalSection& getGridLock() 		{ return columnLock; }
	Buffer<float> getColumnArray() 		{ return dataRetriever->getColumnArray(); }
	const vector<Column>& getColumns() 	{ return dataRetriever->getColumns(); }

protected:

	static const int vertsPerPolygon = 2;
	static const int bytesPerFloat = sizeof(float);

	bool usePixels;

	TextureGL backTex;
	StringArray extensions;
	CriticalSection columnLock;
	Ref<Panel3D::DataRetriever> dataRetriever;

	void populateExtensions();
	bool isSupported(String extension);

	JUCE_LEAK_DETECTOR(OpenGLPanel3D);
};
