#ifndef _vertexpropertiespanel_h
#define _vertexpropertiespanel_h

#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/MouseOverMessager.h>
#include <UI/Widgets/InsetLabel.h>
#include <UI/IConsole.h>
#include "JuceHeader.h"
#include <set>
#include <vector>

#include "../UI/TourGuide.h"

using std::vector;
using std::set;

class Panel;
class TwoTextButton;
class Interactor;
class Knob;
class HSlider;
class Interactor;
class TwoStateButton;
class LabelHint;

class VertexPropertiesPanel :
		public Component
	,	public Slider::Listener
	,	public Button::Listener
	,	public ComboBox::Listener
	,	public TourGuide
	,	public AsyncUpdater
	,	public SingletonAccessor
{
public:
	VertexPropertiesPanel(SingletonRepo* repo);
	~VertexPropertiesPanel();

	Component* getComponent(int which);
	void buttonClicked(Button* button);
	void comboBoxChanged (ComboBox* comboBoxThatHasChanged);
	void drawJustifiedText(Graphics& g, String text, Component* component, int y);
	void drawSidewaysTextAndLines(Graphics& g, String text, Component* top, Component* bottom);
	void gainChanged(Slider* slider, int changeType);
	void getSourceDestDimensionIds(int id, int& srcId, int& destId);
	void handleAsyncUpdate();
	void init();
	void mouseEnter(const MouseEvent& e);
	void mouseUp(const MouseEvent& e);
	void paint(Graphics & g);
	void refreshCube(set<VertCube*>& lines);
	void resized();
	void setSelectedAndCaller(Interactor* interactor);
	void sliderDragEnded(Slider* slider);
	void sliderDragStarted(Slider* slider);
	void sliderValueChanged(Slider* slider);
	void triggerSliderChange(int dim, float value);
	void updateComboBoxes();
	void updateSliderProperties();
	void updateSliderValues(bool ignoreChangeMessage);

	Interactor* getCurrentInteractor() { return currentInteractor; }

private:
	void refreshValueBoxesFromSelected();
	Interactor* currentInteractor;

	Ref<Font> goldfish;
	Ref<Font> silkscreen;

	Label titleLabel;
	Label titleLabelA;

	enum SliderOrder { TimeSldr, KeySldr, ModSldr, PhaseSldr, AmpSldr, CurveSldr, numSliders };
	enum GuideIds { NullDfrmId = 1, /* NewDfrmId */ };
	enum { ValueChanged, DragEnded, DragStarted };


	class GainListener : public Slider::Listener
	{
	public:
		GainListener(VertexPropertiesPanel* panel) : panel(panel) {}
		void sliderValueChanged(Slider* slider) { panel->gainChanged(slider, ValueChanged); }
		void sliderDragEnded(Slider* slider)	{ panel->gainChanged(slider, DragEnded); 	}
		void sliderDragStarted(Slider* slider)	{ panel->gainChanged(slider, DragStarted); 	}

	private:
		Ref<VertexPropertiesPanel> panel;
	};

	class VertexProperties
	{
	public:
		VertexProperties(VertexPropertiesPanel* panel, const String& name, const String& text, int id);

		virtual ~VertexProperties();
		void setValueToCurrent(bool sendChangeMessage);
		void setGainValueToCurrent(bool sendChangeMessage);
		VertexProperties& operator=(const VertexProperties& copy);
		VertexProperties(const VertexProperties& copy);

		int id;
		float previousValue;
		float previousGain;

		HSlider* 			slider;
		ComboBox* 			dfrmChanBox;
		Knob*				gain;
		MouseOverMessager* 	messager;
	};

	GlowEffect glow;
	GainListener gainListener;
	String ampVsPhaseStr;

	Component sliderArea, boxArea, gainArea;

	vector<VertexProperties> properties;
	vector<VertexProperties*> allProperties;
	vector<VertexProperties*> gainProperties;

	std::unique_ptr<VertexProperties> ampVsPhaseProperties;

	InsetLabel title;
};

#endif
