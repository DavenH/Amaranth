#pragma once

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

	void buttonClicked(Button* button) override;
	void init() override;
	void resized() override;
	void paint(Graphics& g) override;
	void paintOverChildren(Graphics& g) override;
	void setNumCommunityPresets(int num);
	void setPlayStopped();
	void setPlayStarted();
	void sliderValueChanged(Slider* slider) override;
	void mouseEnter(const MouseEvent& e) override;
	void updateHighlights();
	void triggerClick(int stage);
	void setLinkHighlight(bool highlit) { linkYellow.setHighlit(highlit); }
	Component* getComponent(int which) override;

	explicit GeneralControls(SingletonRepo* repo);
	~GeneralControls() override = default;

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

	std::unique_ptr<RetractableCallout> waveCO;
	std::unique_ptr<RetractableCallout> envSelectCO;
	std::unique_ptr<RetractableCallout> transCO;
	std::unique_ptr<RetractableCallout> stageCO;
	std::unique_ptr<RetractableCallout> toolCO;
	std::unique_ptr<RetractableCallout> linkCO;
	std::unique_ptr<RetractableCallout> prstCO;

	std::unique_ptr<PulloutComponent> prstPO;
	std::unique_ptr<PulloutComponent> wavePO;
	std::unique_ptr<PulloutComponent> envSelectPO;
	std::unique_ptr<PulloutComponent> transPO;
	std::unique_ptr<PulloutComponent> stagePO;
	std::unique_ptr<PulloutComponent> toolPO;
	std::unique_ptr<PulloutComponent> linkPO;

	Ref<PlaybackPanel> 	position;
	Ref<Dialogs> 		dialogs;
	Ref<FileManager> 	fileManager;
	Ref<AudioSourceRepo> audioManager;

	vector<RetractableCallout*> topCallouts;
	vector<RetractableCallout*> botCallouts;

	static const int consoleHeight = 23;
};
