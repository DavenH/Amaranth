#pragma once
#include "JuceHeader.h"

using namespace juce;

class IDynamicSizeComponent
{
public:
	IDynamicSizeComponent() :
			isCurrentCollapsed(false)
		,	alwaysCollapsed(false)
		,	neverCollapsed(false)
		,	outline(false)
		,	translateUp1(false)
		,	width(24) {}

	virtual ~IDynamicSizeComponent() = default;

	[[nodiscard]] bool isNeverCollapsed() const	{ return neverCollapsed;	}
	[[nodiscard]] bool isAlwaysCollapsed() const { return alwaysCollapsed; 	}
	[[nodiscard]] int getDynWidth() const { return width; 			}

	virtual void setCurrentlyCollapsed(bool isIt) {
        isCurrentCollapsed = isIt && !neverCollapsed;
    }

	virtual bool isCurrentlyCollapsed() {
        return isCurrentCollapsed || alwaysCollapsed;
    }

    /* ----------------------------------------------------------------------------- */

	virtual int getMinorSize() { return 24; }
	virtual void setVisibleDlg(bool isVisible) {}
	[[nodiscard]] virtual bool isVisibleDlg() const { return true; }
	[[nodiscard]] virtual Rectangle<int> getBoundsInParentDelegate() const = 0;
	[[nodiscard]] virtual int getCollapsedSize() const = 0;
	[[nodiscard]] virtual int getExpandedSize() const = 0;
	virtual int getXDelegate() = 0;
	virtual int getYDelegate() = 0;
	virtual void setBoundsDelegate(int x, int y, int w, int h) = 0;

	bool outline, translateUp1;
	int width;

protected:
	bool isCurrentCollapsed;
	bool alwaysCollapsed;
	bool neverCollapsed;

};
