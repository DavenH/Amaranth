#ifndef _playbackpanel_h
#define _playbackpanel_h

#include <App/SingletonAccessor.h>
#include <Curve/Vertex.h>
#include <Inter/Dimensions.h>
#include <Obj/Ref.h>
#include <UI/Widgets/IconButton.h>
#include "JuceHeader.h"
#include <Util/MicroTimer.h>
#include "../../Curve/GraphicRasterizer.h"

class Mesh;
class Spectrum2D;
class Spectrum3D;
class MorphPanel;
class MainPanel;

class PlaybackPanel :
		public Button::Listener
	,	public Component
	,	public MultiTimer
	,	public SingletonAccessor
{
public:
	enum
	{
		MainTimerId
	,	ReleaseTimerId
	};

	PlaybackPanel(SingletonRepo* repo);
	~PlaybackPanel();

	void paint(Graphics& g);
	void resized();
	void mouseEnter(const MouseEvent& e);
	void mouseDrag(const MouseEvent& e);
	void mouseDown(const MouseEvent& e);
	void mouseUp(const MouseEvent& e);

	void update(const MouseEvent& e);
	void getRange(float& lower, float& upper);
	void primaryDimensionChanged();
	void buttonClicked(Button* button);

	void init();
	float getProgress();
	void setProgress(float unitX, bool updateMorphPanel = true);

	void doPostCallback();
	void updateView();

	void resetPlayback(bool doUpdate);
	void stopPlayback();
	bool startPlayback();
	bool isPlaying();

	void update();
	void stopAudio();
	void startAudio();
	void setAudioPosition();
	void setXAndUpdate(float pos);
	float getX();
	void togglePlayback();
	void timerCallback(int id);
	float getScratchPosition(int scratchChannel);
	double getEnvelopePos() { return envPos; }

private:
	static const int timeDelay = 10;

	bool playing;
	bool playingAudio;
	bool haveSpeed;

	float x;
	float end;
	float incrementScale;
	double envPos;

	int speedIndex;
	int midiKeyPlaying;

	MicroTimer timer;

	Ref<MainPanel> 			main;
	Ref<MorphPanel> 		morphPanel;
	Ref<Spectrum2D> 		spectrum2D;
	Ref<Spectrum3D> 		spectrum3D;
	Ref<TimeRasterizer> 	timeRasterizer;
	Ref<SpectRasterizer> 	freqRasterizer;
	Ref<PhaseRasterizer> 	phaseRasterizer;

	IconButton attkZoomIcon;
	IconButton zoomOutIcon;

	Image scrollCursor;
	Image eye;
};


#endif
