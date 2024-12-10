#pragma once
#include "Panel.h"

#include "../../Obj/ColorGradient.h"
#include "../../Array/Column.h"

using std::vector;

class Interactor3D;
class OpenGLPanel3D;
class BoundWrapper;

class Panel3D : public Panel {
public:
    class Renderer : public Panel::Renderer {
	public:
		virtual void deactivate() 	= 0;
		virtual void activate() 	= 0;
		virtual void clear() 		= 0;

		virtual void drawBakedTextures() 	{}
		virtual void gridDrawBeginning() 	{}	// enable client arrays
		virtual void gridDrawFinished() 	{}	// disable client arrays
		virtual void postVertsDraw() 		{}
		virtual void scratchChannelSelected(int ch) {}
		virtual void textureBakeBeginning() {}
		virtual void textureBakeFinished() 	{}

		virtual bool isDetailReduced() 		{ return false; }
		virtual bool isScratchApplicable() 	{ return false; }
		virtual bool shouldDrawGrid() 	 	{ return true;  }
		virtual bool willAdjustColumns() 	{ return false; }

		virtual Buffer<float> getColumnArray() 		= 0;
		virtual const vector<Column>& getColumns() 	= 0;
		virtual CriticalSection& getGridLock() 		= 0;
		virtual void drawSurfaceColumn(int index) 	= 0;
	};


	class DataRetriever {
	public:
		virtual Buffer<float> getColumnArray() = 0;
		virtual const vector<Column>& getColumns() = 0;
	};

    /* ----------------------------------------------------------------------------- */

	Panel3D(SingletonRepo* repo, const String& name,
			DataRetriever* dataRetriever,
			bool isTransparent, bool haveHorzZoom);

	virtual ~Panel3D();

	void bakeTextures();
	void doExtraResized();
	void drawAxe();
	void drawDeformerTags();
	void drawDepthLinesAndVerts();
	void drawInterceptLines();
	void drawInterceptsAndHighlightClosest();
	void drawLinSurface(const vector<Column>& grid);
	void drawLogSurface(const vector<Column>& grid);
	void drawSurface();
	void freeResources();
	void highlightCurrentIntercept();
	vector<Color>& getGradientColours();
	void zoomUpdated(int updateSource);

	void drawCurvesAndSurfaces() 					{ renderer->drawBakedTextures(); }
	void setGraphicsRenderer(Renderer* renderer) 	{ this->renderer = renderer; }
	Renderer* getRenderer() 						{ return renderer; }
	void setUseVertices(bool doso) 					{ useVertices = doso; }
	OpenGLPanel3D* getOpenglPanel() 				{ return openGL; }

	virtual void postVertsDraw();

protected:
	struct RenderParams {
		bool adjustColumns;
		bool reduced;

		int texHeight;
		int currKey, lastKey;
		int downsampSize;
		int sizeX, sizeY, lastSizeY, colSourceSizeY;
		int stride;

		Buffer<float> ramp;
	};

	void adjustLogColumnAndScale(int key, bool firstColumn);
	void doColourLookup32f		(Buffer<float> grd, Buffer<float> colors);
	void doColourLookup8u		(Buffer<Ipp8u> grd, Buffer<Ipp8u> colors);
	void doColumnDraw			(Buffer<Ipp8u> grd8u, Buffer<float> grd32f, int i);
	void downsampleColumn		(Buffer<float> column);
	void resampleColours		(Buffer<float> srcColors, Buffer<float> dstColors);
	void setVertices			(int column, Buffer<float> verts);
	void resizeArrays			();
	void setColumnColourIndices	();

    /* ----------------------------------------------------------------------------- */

	static const int bytesPerFloat 	 = sizeof(float);
	static const int maxHorzLines 	 = 512;
	static const int vertsPerQuad 	 = 2;
	static const int coordsPerVert	 = 2;
	static const int gradientWidth	 = 512;

	bool useVertices, haveLogarithmicY;
	float volumeScale, volumeTrans;

	Ref<Interactor3D> interactor3D;
	std::unique_ptr<Renderer> renderer;
	std::unique_ptr<OpenGLPanel3D> openGL;
	std::unique_ptr<BoundWrapper> wrapper;

	ColorGradient gradient;
	RenderParams draw;

	ScopedAlloc<Ipp16s> clrIndicesA, clrIndicesB;
	ScopedAlloc<Ipp32f> downsampAcc, downsampBuf, scaledX, scaledY;
	ScopedAlloc<Ipp32f> fColours, fDestColours, vertices;
	ScopedAlloc<Ipp8u> 	colours, texColours;

    /* ----------------------------------------------------------------------------- */

	friend class OpenGLPanel3D;
	friend class JPanel3D;

	JUCE_LEAK_DETECTOR(Panel3D);
};
