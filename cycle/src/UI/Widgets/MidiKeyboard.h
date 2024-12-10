#ifndef _midikeyboard_h
#define _midikeyboard_h

#include "JuceHeader.h"
#include <Definitions.h>
#include <Obj/Ref.h>
#include <App/SingletonAccessor.h>

class SingletonRepo;

class MidiKeyboard :
		public MidiKeyboardComponent
	,	public SingletonAccessor {
public:
	friend class MidiKeyboardComponent;

	MidiKeyboard(SingletonRepo* repo, MidiKeyboardState& state, const Orientation orientation);
	~MidiKeyboard();

	void mouseExit(const MouseEvent& e);
	void mouseEnter(const MouseEvent& e);
	void mouseMove(const MouseEvent& e);
	static String getText(int note);

	float getVelocityA();
	int getAuditionKey() { return auditionKey; }
	void resized();
	void setAuditionKey(int key);
	void setMidiRange(const Range<int>& range);

protected:
	void drawBlackNote(int midiNoteNumber, Graphics &g, int x, int y, int w, int h, bool isDown,
			bool isOver, const Colour& noteFillColour);

	void drawWhiteNote(int midiNoteNumber, Graphics &g, int x, int y, int w, int h, bool isDown,
			bool isOver, const Colour& lineColourDefault,
			const Colour &textColour);

	void drawUpDownButton(Graphics& g, int w, int h, const bool isMouseOver, const bool isButtonPressed,
			const bool movesOctavesUp);

	void getKeyPosition(int midiNoteNumber, float keyWidth, int& x, int& w) const;
	bool mouseDownOnKey(int midiNoteNumber, const MouseEvent& e);
	void mouseDraggedToKey(int midiNoteNumber, const MouseEvent& e);
	void updateNote(const Point<int>&);

private:
    enum {
        pxGreyBlack 	= 0,
		pxGreyWhite 	= 8,
		pxBlueBlack 	= 20,
		pxBlueWhite 	= 28,
		pxOrangeBlack 	= 40,
		pxOrangeWhite 	= 48,
	};

	int getMouseNote();
	void setMouseNote(int note);
	bool shouldDrawAuditionKey(int note);

	GlowEffect glow;
	Image keys;

	Range<int> midiRange;
	int auditionKey;

	JUCE_LEAK_DETECTOR(MidiKeyboard)
};

#endif
