#ifndef _BACKGROUNDPANEL_H_
#define _BACKGROUNDPANEL_H_

#include <UI/MiscGraphics.h>

class BackgroundPanel : public Component {
public:
    void paint(Graphics& g) {
        getObj(CycleGraphicsUtils).fillBlackground(this, g);
    }
};


#endif
