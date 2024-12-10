#ifndef _keyboardinputhandler_h
#define _keyboardinputhandler_h

#include <Obj/Ref.h>
#include "JuceHeader.h"
#include <App/SingletonAccessor.h>

class Mesh;
class Dialogs;
class PlaybackPanel;
class Waveform3D;
class Waveform2D;
class WaveformInter3D;
class WaveformInter2D;
class Interactor;
class MainPanel;
class Console;

class KeyboardInputHandler :
		public KeyListener
	,	public SingletonAccessor
{
public:
	enum SpecialKeys
	{
		CtrlA		= 65,
		CtrlF		= 70,
		CtrlL 		= 76,
		CtrlN		= 78,
		CtrlP 		= 80,
		CtrlR		= 82,
		CtrlS 		= 83,
		CtrlZ 		= 90,

		Delete 		= 65582,
		End			= 65571,
		Home		= 65572,
		PageUp		= 65569,
		PageDown	= 65570,
		LeftArrow	= 65573,
		RightArrow	= 65575
	};

	KeyboardInputHandler(SingletonRepo* repo);

	void init();
	bool keyPressed(const KeyPress& key, Component* component);
	void setFocusedInteractor(Interactor* interactor, bool);
	Interactor* getCurrentInteractor();

private:
	Ref<Mesh> mesh;
	Ref<PlaybackPanel> position;
	Ref<WaveformInter3D> waveInter3D;
	Ref<WaveformInter2D> wave2DInteractor;
	Ref<Waveform3D> surface;
	Ref<Waveform2D> wave2D;
	Ref<MainPanel> main;
	Ref<Console> console;

//	Panel* currentPanel;
	bool isMeshInteractor;
	Interactor* currentInteractor;
};

#endif
