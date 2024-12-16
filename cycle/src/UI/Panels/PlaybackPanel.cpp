#include <App/AppConstants.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/Multisample.h>
#include <Design/Updating/Updater.h>
#include <UI/IConsole.h>
#include <UI/MiscGraphics.h>
#include <Audio/AudioSourceRepo.h>
#include <Definitions.h>

#include "../../Inter/EnvelopeInter2D.h"
#include "../../Inter/SpectrumInter2D.h"
#include "../../Inter/WaveformInter2D.h"

#include "PlaybackPanel.h"

#include "../VisualDsp.h"

#include "MainPanel.h"
#include "Morphing/MorphPanel.h"

#include "../Panels/DerivativePanel.h"
#include "../Panels/GeneralControls.h"
#include "../Panels/OscControlPanel.h"
#include "../VertexPanels/Envelope2D.h"
#include "../VertexPanels/Envelope3D.h"
#include "../VertexPanels/Spectrum2D.h"
#include "../VertexPanels/Spectrum3D.h"
#include "../VertexPanels/Waveform2D.h"
#include "../VertexPanels/Waveform3D.h"
#include "../Widgets/MidiKeyboard.h"
#include "../../Audio/WavAudioSource.h"
#include "../../Util/CycleEnums.h"
#include "../CycleDefs.h"

PlaybackPanel::PlaybackPanel(SingletonRepo* repo) :
		SingletonAccessor(repo, "PlaybackPanel")
	,	attkZoomIcon(6, 2, this, repo, "Zoom to attack")
	,	zoomOutIcon( 6, 3, this, repo, "Zoom out to sustain region")
{
	eye = getObj(MiscGraphics).getIcon(3, 7);

	addAndMakeVisible(&attkZoomIcon);
	addAndMakeVisible(&zoomOutIcon);
}

void PlaybackPanel::buttonClicked(Button* button) {
	if (button == &attkZoomIcon) {
		getObj(Envelope2D).getZoomPanel()->zoomToAttack();
	} else if (button == &zoomOutIcon) {
		getObj(Envelope2D).getZoomPanel()->zoomToFull();
	}

	repaint();
}

void PlaybackPanel::paint(Graphics& g) {
	g.fillAll(Colour::greyLevel(0.1f));

	float xOffset = 64;

	//TODO

	Waveform3D* waveform3D = &getObj(Waveform3D);

	float x 	 = getObj(PlaybackPanel).getX();
	float dx 	 = getObj(MorphPanel).getPrimaryViewDepth();
	float minX 	 = waveform3D->sx(jmax(0.f, x)) + xOffset;
	float maxX 	 = waveform3D->sx(jmin(1.f, x + dx)) + xOffset;
	float sx 	 = waveform3D->sx(x) + xOffset;
	float width  = maxX - minX;
	float offset = 0;
	float height = getHeight() - offset;

	ColourGradient gradient;
	gradient.isRadial = false;
	gradient.point1.setXY(minX, 0);
	gradient.point2.setXY(maxX, 0);
	gradient.addColour(0, Colour(15, 15, 19));
	gradient.addColour(1, Colours::black);

	{
		Graphics::ScopedSaveState sss(g);

		g.reduceClipRegion(Rectangle<int>(xOffset, 0, getWidth() - xOffset, getHeight()));

		g.setColour(Colour(150, 120, 50));
		g.drawVerticalLine(maxX, offset, height);
		g.fillEllipse(maxX - 1.f, height - 2.f, 3.f, 3.f);

		g.setColour(Colours::white);
		g.drawVerticalLine(sx, 0, height);
		g.fillEllipse(sx - 1.f, -1.5f, 3.f, 3.f);

		g.setColour(Colour::greyLevel(0.4f));
		g.setOpacity(0.5f);
		g.fillRect(minX, offset, width, (float)height);

		{
			Graphics::ScopedSaveState sss2(g);

			g.reduceClipRegion(Rectangle<int>(minX, 0, width, getHeight()));
			g.drawImageAt(eye, sx + (width - eye.getWidth()) / 2, (height - eye.getHeight()) / 2);
		}
	}
}

void PlaybackPanel::resized()
{
	Rectangle<int> bounds = getLocalBounds();
	Rectangle<int> buttonBounds = bounds.removeFromLeft(64);

	buttonBounds.reduce(2, 1);
	attkZoomIcon.setBounds(buttonBounds.removeFromLeft(24));

	buttonBounds.removeFromLeft(8);
	zoomOutIcon.setBounds(buttonBounds);
}

void PlaybackPanel::mouseDown(const MouseEvent& e) {
	update(e);
	repaint();
}

void PlaybackPanel::mouseEnter(const MouseEvent& e) {
	getObj(IConsole).updateAll("Set playback position (left), Set view depth (right)",
	                           {}, MouseUsage(true, false, true, true));
}

void PlaybackPanel::mouseDrag(const MouseEvent& e) {
	update(e);
	repaint();
}

void PlaybackPanel::mouseUp(const MouseEvent& e) {
	getObj(MorphPanel).repaint();
}

void PlaybackPanel::update(const MouseEvent& e) {
//	float xOffset = getObj(Waveform3D).getControlsPanel()->getWidth();
	float xOffset = 64;

	float x = getObj(Waveform3D).invertScaleX(e.x - xOffset);
	NumberUtils::constrain<float>(x, 0, 1);

	int dim = getSetting(CurrentMorphAxis);

	if (e.mods.isLeftButtonDown()) {
		getObj(PlaybackPanel).setProgress(x, true);
	} else if (e.mods.isRightButtonDown()) {
		float pos = getObj(PlaybackPanel).getX();
		getObj(MorphPanel).setViewDepth(dim, jmax(0.f, x - pos));

		getObj(Waveform2D).repaint();
		getObj(Spectrum2D).repaint();
		getObj(MorphPanel).repaint();
	} else if (e.mods.isMiddleButtonDown()) {
		if (getObj(MorphPanel).getDepth(dim) == 0)
			return;

		getObj(MorphPanel).setViewDepth(dim, 0);

		getObj(Spectrum2D).repaint();
		getObj(Waveform2D).repaint();
	}
}


//void PlaybackPanel::primaryDimensionChanged()
//{
//	repaint();
//}


/*

PlaybackPanel::GlobalPlaybackPosition(SingletonRepo* repo) :
		SingletonAccessor(repo, "")
	,	x(0)
	,	envPos(0)
	,	playing(false)
	,	incrementScale(1.f)
{
	midiKeyPlaying = 64;
}
*/


void PlaybackPanel::init()
{
	main 			 = &getObj(MainPanel);
	timeRasterizer 	 = &getObj(TimeRasterizer);
	freqRasterizer 	 = &getObj(SpectRasterizer);
	phaseRasterizer  = &getObj(PhaseRasterizer);
	morphPanel 		 = &getObj(MorphPanel);

	x = 0;
	playing = false;
}

void PlaybackPanel::updateView() {
	getObj(WaveformInter2D).performUpdate(Update);
	getObj(WaveformInter2D).locateClosestElement();
	getObj(SpectrumInter2D).performUpdate(Update);
	getObj(SpectrumInter2D).locateClosestElement();
	getObj(Spectrum3D).repaint();
	getObj(Waveform3D).repaint();

	if(getSetting(CurrentMorphAxis) == Vertex::Time) {
		getObj(Envelope2D).repaint();
	} else {
		getObj(Envelope3D).repaint();
	}

	getObj(DerivativePanel).repaint();
	getObj(PlaybackPanel).repaint();
}

void PlaybackPanel::doPostCallback() {
	morphPanel->updateCurrentSliderNoCallback(getProgress());

	timeRasterizer->pullModPositionAndAdjust();
	timeRasterizer->setNoiseSeed(int(timeRasterizer->getMorphPosition().time * 12541));
	timeRasterizer->performUpdate(Update);

	freqRasterizer->pullModPositionAndAdjust();
	freqRasterizer->setNoiseSeed(int(freqRasterizer->getMorphPosition().time * 33947));
	freqRasterizer->performUpdate(Update);

	phaseRasterizer->pullModPositionAndAdjust();
	phaseRasterizer->setNoiseSeed(int(phaseRasterizer->getMorphPosition().time * 94121));
	phaseRasterizer->performUpdate(Update);

	getObj(DerivativePanel).calcDerivative();
}

float PlaybackPanel::getProgress() const {
	return x;
}

void PlaybackPanel::setProgress(float unitX, bool updateMorphPanel) {
	if (updateMorphPanel)
		morphPanel->updateCurrentSliderNoCallback(unitX);

	x = jlimit(0.f, 1.f, unitX);

	if (EnvRasterizer* envRast = getObj(EnvelopeInter2D).getEnvRasterizer()) {
		MeshLibrary::EnvProps* props = getObj(MeshLibrary).getCurrentEnvProps(getSetting(CurrentEnvGroup));

		if (props->active) {
			envPos = 0;
			envRast->simulateRender(x, envPos, *props, 1.f);
		}
	}

	setAudioPosition();
	doPostCallback();
	updateView();
}

void PlaybackPanel::setXAndUpdate(float pos) {
	x = jlimit(0.f, 1.f, pos);

	doPostCallback();
	updateView();
}

void PlaybackPanel::stopPlayback() {
	stopTimer(MainTimerId);

	if (playing) {
		if (EnvRasterizer* envRast = getObj(EnvelopeInter2D).getEnvRasterizer()) {
			if(envRast->simulateStop(envPos))
				startTimer(ReleaseTimerId, timeDelay);
		}
	}

	stopAudio();
	getObj(GeneralControls).setPlayStopped();

	playing = false;
}

bool PlaybackPanel::startPlayback() {
	bool canStart = false;

	if(getSetting(DrawWave)) {
		canStart = getSetting(WaveLoaded) == 1;
		incrementScale = 1.f / getObj(Multisample).getGreatestLengthSeconds();
	} else {
		incrementScale = 1.f / getObj(OscControlPanel).getLengthInSeconds();

		canStart = timeRasterizer->hasEnoughCubesForCrossSection() ||
				   freqRasterizer->hasEnoughCubesForCrossSection();
	}

	if (x >= 1.f) {
		x = 0;
	}

	if(EnvRasterizer* envRast = getObj(EnvelopeInter2D).getEnvRasterizer()) {
		envRast->simulateStart(envPos);
	}

	if (canStart) {
		playing = true;

		startAudio();
		timer.start();
		startTimer(MainTimerId, timeDelay);
		timerCallback(MainTimerId);
		getObj(GeneralControls).setPlayStarted();
	} else {
		playing = false;
	}

	return canStart;
}

void PlaybackPanel::setAudioPosition() {
	if (playing && getSetting(DrawWave)) {
		getObj(AudioSourceRepo).getWavAudioSource()->positionChanged();
	}
}

void PlaybackPanel::resetPlayback(bool doUpdate) {
	stopPlayback();

	x = 0;

	if (getSetting(DrawWave)) {
		WavAudioSource* waveSource = getObj(AudioSourceRepo).getWavAudioSource();

		if(waveSource != nullptr) {
			waveSource->positionChanged();
		}
	}

	if (doUpdate)
	{
		doPostCallback();
		updateView();
	}
}

void PlaybackPanel::update() {
}

void PlaybackPanel::startAudio() {
	if (x < 1) {
		midiKeyPlaying = getObj(MidiKeyboard).getAuditionKey();
		NumberUtils::constrain(midiKeyPlaying, getConstant(LowestMidiNote), getConstant(HighestMidiNote));

		float velocity = 0.8f;

		if(getSetting(DrawWave)) {
			velocity = 1.f - getObj(MorphPanel).getValue(Vertex::Blue);
		}

		getObj(AudioHub).getKeyboardState().noteOn(1, midiKeyPlaying, jmax(0.01f, velocity));
	}
}

void PlaybackPanel::stopAudio() {
	if (playing) {
		getObj(AudioHub).getKeyboardState().noteOff(1, midiKeyPlaying, 0.8f);
	}
}

float PlaybackPanel::getX() {
	return x;
}

void PlaybackPanel::togglePlayback() {
	if (playing) {
		stopPlayback();
		resetPlayback(true);
	} else {
		stopTimer(ReleaseTimerId);
		resetPlayback(x > 0.999f);
		startPlayback();
	}
}

void PlaybackPanel::timerCallback(int id) {
	jassert(! isTimerRunning(MainTimerId) || ! isTimerRunning(ReleaseTimerId));

	timer.stop();
	float delta = timer.getDeltaTime();
	float inc = incrementScale * delta;

	timer.start();

	if (id == MainTimerId) {
		x = jmin(1.f, x + inc);

		if(x == 1.f)
			stopPlayback();
		else
		{
			doPostCallback();
			updateView();
		}
	}

	EnvRasterizer* envRast = getObj(EnvelopeInter2D).getEnvRasterizer();
	if (envRast == nullptr) {
		stopTimer(ReleaseTimerId);
	} else {
		MeshLibrary::EnvProps* props = getObj(MeshLibrary).getCurrentEnvProps(getSetting(CurrentEnvGroup));
		bool isAlive = props->active;

		if(isAlive) {
			isAlive = envRast->simulateRender(inc, envPos, *props, 1.f);
		}

		if(! isAlive) {
			stopTimer(ReleaseTimerId);
		}

		if(id == ReleaseTimerId) {
			getObj(Envelope2D).repaint();
		}
	}
}

bool PlaybackPanel::isPlaying() {
	return playing;
}

void PlaybackPanel::primaryDimensionChanged() {
	setXAndUpdate(morphPanel->getValue(getSetting(CurrentMorphAxis)));
}

float PlaybackPanel::getScratchPosition(int scratchChannel) {
	bool useSpeedEnv = getSetting(CurrentMorphAxis) == Vertex::Time;

	if (useSpeedEnv) {
		float scratch = getObj(VisualDsp).getScratchPosition(scratchChannel);
		return scratch;
	}

	return x;
}
