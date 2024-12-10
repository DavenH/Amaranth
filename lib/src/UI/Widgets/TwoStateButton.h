#pragma once

#include "IconButton.h"

class TwoStateButton :
	public IconButton {
public:

	enum { First, Second };

	TwoStateButton(int x1, int y1, int x2, int y2,
				   Button::Listener* listener,
				   SingletonRepo* repo,
				   const String& msgA, const String& keyA,
				   const String& msgB, const String& keyB);

	~TwoStateButton();
	void paintButton(Graphics& g, bool mouseOver, bool buttonDown) override;
	void setState(int state);
	void toggle();
	void setMessages(String one, String key1, String two, String key2);
	bool isFirstState();
	void mouseDown(const MouseEvent& e) override;

protected:
	JUCE_LEAK_DETECTOR(TwoStateButton)

	Image second;
	int state;
	String secondMessage;
	String secondKey;
};