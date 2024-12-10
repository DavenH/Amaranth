#ifndef MOUSEEVENTDELEGATOR_H_
#define MOUSEEVENTDELEGATOR_H_

#include "JuceHeader.h"
#include "../Util/Util.h"
#include "../Obj/Ref.h"

class MouseEventDelegatee {
public:
	virtual void enterDlg() = 0;
	virtual void exitDlg() = 0;
};

#ifdef JUCE_MAC
class MouseEventDelegator :
		public MouseListener
	,	public Timer
{
public:
	MouseEventDelegator(Component* comp, MouseEventDelegatee* delegatee) :
			isOver(false)
		,	comp(comp)
		, 	delegatee(delegatee)
	{
		Desktop::getInstance().addGlobalMouseListener (this);
	}

	~MouseEventDelegator()
	{
		Desktop::getInstance().removeGlobalMouseListener (this);
	}

	void mouseMove(const MouseEvent& e)
	{
		if(! comp->isVisible())
			return;

		const Point<int> globalMousePos (Desktop::getMousePosition());
		const Point<int> localMousePos (comp->getLocalPoint (nullptr, globalMousePos));

		const uint32 now = Time::getMillisecondCounter();

		if(Util::assignAndWereDifferent(isOver, comp->reallyContains (localMousePos, true)))
		{
			if(isOver)
			{
				startTimer(10);
			}
			else
			{
				if(tempComponent != nullptr)
				{
					tempComponent->setMouseCursor(tempCursor);
				}

				delegatee->exitDlg();
			}
		}
	}

	void timerCallback()
	{
		tempComponent = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();

		if(tempComponent != nullptr)
		{
			tempCursor = tempComponent->getMouseCursor();
			tempComponent->setMouseCursor(MouseCursor::PointingHandCursor);
		}

		Desktop::getInstance().getMainMouseSource().forceMouseCursorUpdate();

		stopTimer();
		delegatee->enterDlg();
	}

private:

	Ref<Component> comp;
	Ref<Component> tempComponent;
	MouseCursor tempCursor;
	Ref<MouseEventDelegatee> delegatee;
	bool isOver;
};

#else
class MouseEventDelegator {
public:
    MouseEventDelegator(Component* comp, MouseEventDelegatee* delegatee) {
	}
};
#endif


#endif
