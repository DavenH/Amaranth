#ifndef _generalcontrols_h
#define _generalcontrols_h

#include "JuceHeader.h"
#include <vector>
#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/TwoStateButton.h>

#include "../TourGuide.h"

class Envelope2D;
class EnvelopeInter2D;
class PlaybackPanel;
class Dialogs;
class AudioSourceRepo;
class PulloutComponent;
class RetractableCallout;
class FileManager;

using std::vector;

class GeneralControls :
		public Component
	,	public Button::Listener
	,	public Slider::Listener
	,	public TourGuide
	, 	public SingletonAccessor
{
public:
	enum Stages { AStage, BStage, CStage, DStage };
	enum { CompSelector, CompPencil, CompAxe, CompNudge, CompWaveVerts, CompVerts, CompLinkYellow };

	void buttonClicked(Button* button);
	void init();
	void resized();
	void paint(Graphics& g);
	void paintOverChildren(Graphics& g);
	void setNumCommunityPresets(int num);
	void setPlayStopped();
	void setPlayStarted();
	void sliderValueChanged(Slider* slider);
	void mouseEnter(const MouseEvent& e);
	void updateHighlights();
	void triggerClick(int stage);
	void setLinkHighlight(bool highlit) { linkYellow.setHighlit(highlit); }
	Component* getComponent(int which);

	GeneralControls(SingletonRepo* repo);
	virtual ~GeneralControls();

private:
	static const int bSize = 24;

	IconButton verts;
	IconButton waveVerts;

	// transport
	TwoStateButton play;
	IconButton rewind;
	IconButton ffwd;

	// action buttons
	IconButton save;
	IconButton prev;
	IconButton next;

	IconButton pointer;
	IconButton pencil;
	IconButton axe;
	IconButton nudge;
	IconButton tableIcon;
	IconButton linkYellow;

	friend class EnvelopeInter2D;
	friend class Envelope2D;

	ScopedPointer<RetractableCallout> waveCO;
	ScopedPointer<RetractableCallout> envSelectCO;
	ScopedPointer<RetractableCallout> transCO;
	ScopedPointer<RetractableCallout> stageCO;
	ScopedPointer<RetractableCallout> toolCO;
	ScopedPointer<RetractableCallout> linkCO;
	ScopedPointer<RetractableCallout> prstCO;

	ScopedPointer<PulloutComponent> prstPO;
	ScopedPointer<PulloutComponent> wavePO;
	ScopedPointer<PulloutComponent> envSelectPO;
	ScopedPointer<PulloutComponent> transPO;
	ScopedPointer<PulloutComponent> stagePO;
	ScopedPointer<PulloutComponent> toolPO;
	ScopedPointer<PulloutComponent> linkPO;

	Ref<PlaybackPanel> 	position;
	Ref<Dialogs> 		dialogs;
	Ref<FileManager> 	fileManager;
	Ref<AudioSourceRepo> audioManager;

	vector<RetractableCallout*> topCallouts;
	vector<RetractableCallout*> botCallouts;

	static const int consoleHeight = 23;

	friend class GlobalOperations;
};

#endif
