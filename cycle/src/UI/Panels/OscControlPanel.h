#ifndef _osccontrolpanel_h
#define _osccontrolpanel_h

#include <App/Doc/EditSource.h>
#include <App/Doc/Savable.h>
#include <App/SingletonAccessor.h>
#include <map>
#include <Obj/Ref.h>
#include <UI/AsyncUIUpdater.h>
#include <UI/ParameterGroup.h>
#include <Util/Arithmetic.h>
#include "JuceHeader.h"
#include "../TourGuide.h"
#include "../Widgets/HSlider.h"


using std::map;

class Knob;
class SynthAudioSource;

class OscControlPanel:
		public Component
	,	public ParameterGroup::Worker
	,	public Savable
	,	public Timer
	,	public TourGuide
	,	public SingletonAccessor
{
public:
	enum HSliders
	{
		Volume
	,	Octave
	,	Speed
	,	numSliders
	};

	OscControlPanel(SingletonRepo* repo);

	/* UI */
	void paint(Graphics& g);
	void resized();
	void init();

	/* Persistence */
	void writeXML(XmlElement* element) const;
	bool readXML(const XmlElement* element);

	/* Events */
	bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate);
	bool shouldTriggerGlobalUpdate(Slider* slider);
	void restoreDetail();
	void reduceDetail();
	void doGlobalUIUpdate(bool force);
	void sliderValueChanged (Slider* slider);
	void setLengthInSeconds(float value);
	void timerCallback();
	Component* getComponent(int);

	float getVolumeScale() 				{ return scaleVolume((float) volume->getValue()); }
	float getLengthInSeconds() const 	{ return scaleSpeed((float) speed->getValue()); }

	static float scaleVolume	(float val) { return expf(6 * val - 3); }
	static float scaleOctave	(float val) { return (float) ::roundToInt(4 * (val - 0.5f) + 0.5f); }
	static float scaleSpeed		(float val) { return expf(8 * val - 3); }
	static float scaleSpeedInv	(float val) { return ((float) log(val) + 3) / 8.f; }
	static float scalePhase		(float val) { return val; }
//	static float cutoffScale	(float val) { return Arithmetic::invLogMapping((float) getConstant(AmpTension), val); }

private:
	bool doUpdateWithSpeedSlider();

	float currentVolumeLevel;

	Ref<SynthAudioSource> synth;
	Ref<HSlider> volume;
	Ref<HSlider> octave;
	Ref<HSlider> speed;
	Ref<HSlider> phase;

	HSlider* hSliders[numSliders];
};

#endif
