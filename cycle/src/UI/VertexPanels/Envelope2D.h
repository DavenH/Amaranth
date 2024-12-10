#ifndef _envelopepanel_h
#define _envelopepanel_h

#include <App/SingletonAccessor.h>
#include <App/Doc/Savable.h>
#include <Curve/EnvRasterizer.h>
#include <Obj/Ref.h>

#include <UI/Panels/Panel2D.h>
#include "../UI/TourGuide.h"

class Waveform3D;
class E3Rasterizer;
class EnvelopeInter3D;
class EnvelopeInter2D;
class OscControlPanel;
class PulloutComponent;
class RetractableCallout;
class EditWatcher;
class EnvelopeMesh;

template<class EnvelopeMesh> class MeshSelector;

class Envelope2D:
		public Panel2D
	,	public TourGuide
//	,	public Savable
{
	friend class EnvelopeInter3D;

public:
	class ScrollListener : public MouseListener
	{
	public:
		Envelope2D* panel;

		ScrollListener(Envelope2D* panel) : panel(panel) {}
		void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel);
	};


	class Controls : public Component, public SingletonAccessor
	{
	public:
		Controls(SingletonRepo* repo, Envelope2D* panel);

		void init();
		void destroy();
		void resized();
		void paint(Graphics& g);
		void paintOverChildren(Graphics& g);
		void setSelectorVisibility(bool isVisible, bool doRepaint);

		bool				showLayerSelector;
		Ref<Envelope2D> 	panel;
	};

	Envelope2D(SingletonRepo* repo);
	~Envelope2D();

	/* */
	void init();
	void panelResized();
	void setZoomLimit(float limit) { zoomPanel->rect.xMaximum = limit; }

	/* drawing */
	void drawCurvesAndSurfaces();
	void setListenersForEditWatcher(EditWatcher* watcher);

	/* callbacks */
//	bool readXML(const XmlElement* element);
	Component* getComponent(int which);
	Component* getControlsComponent() { return &controls; }

	float getLoopStart();

	void componentChanged();
	void createScales();
	void dimensionsChanged();
	void drawBackground(bool fill);
	void drawMouseHint();
	void drawRatioLines();
	void drawScales();
	void getLoopPoints(float& loopStart, float& sustain);
	void postCurveDraw();
//	void removeScratchProps(int index);
	void setSelectorVisibility(bool isVisible, bool doRepaint = true);
	void updateBackground(bool verticalOnly);
	void zoomAndRepaint();

	friend class Controls;

	enum { nullEnvIndex = -1 };

private:
	ScrollListener scrollListener;

	Ref<EnvelopeInter2D> e2Interactor;
	Ref<Waveform3D> surface;

	Controls controls;

//	MeshLibrary::EnvProps defaultProps;
//	MeshLibrary::EnvProps volumeProps;
//	MeshLibrary::EnvProps pitchProps;
//	Array<MeshLibrary::EnvProps> scratchProps;

	ScopedPointer<MeshSelector<EnvelopeMesh> > meshSelector;
	ScopedPointer<RetractableCallout> 	envSelectCO;
	ScopedPointer<PulloutComponent> 	envSelectPO;

	ScopedPointer<RetractableCallout> 	loopCO;
	ScopedPointer<PulloutComponent> 	loopPO;

	friend class ScrollListener;
	friend class EnvelopeInter2D;
};

#endif
