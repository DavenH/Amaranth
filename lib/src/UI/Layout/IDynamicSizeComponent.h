#pragma once
#include "JuceHeader.h"

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

	virtual ~IDynamicSizeComponent() {}

	bool isNeverCollapsed()		{ return neverCollapsed;	}
	bool isAlwaysCollapsed() 	{ return alwaysCollapsed; 	}
	int getDynWidth() 			{ return width; 			}

	void setCurrentlyCollapsed(bool isIt) {
        isCurrentCollapsed = isIt && !neverCollapsed;
    }

    bool isCurrentlyCollapsed() {
        return isCurrentCollapsed || alwaysCollapsed;
    }

    /* ----------------------------------------------------------------------------- */

	virtual bool isVisibleDlg() const 			{ return true; 	}
	virtual int getMinorSize() 					{ return 24; 	}
	virtual void setVisibleDlg(bool isVisible) 	{}

	virtual const Rectangle<int> getBoundsInParentDelegate() = 0;
	virtual int getCollapsedSize() 	= 0;
	virtual int getExpandedSize() 	= 0;
	virtual int getXDelegate() 		= 0;
	virtual int getYDelegate() 		= 0;
	virtual void setBoundsDelegate(int x, int y, int w, int h) = 0;

	bool outline, translateUp1;
	int width;

protected:
	bool isCurrentCollapsed;
	bool alwaysCollapsed;
	bool neverCollapsed;

};
