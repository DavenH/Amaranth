#pragma once

#include "../Obj/MorphPosition.h"

class MorphPositioner {
public:
    virtual ~MorphPositioner() = default;

    virtual MorphPosition getMorphPosition()    { return {}; }
    virtual MorphPosition getOffsetPosition(bool withDepths) { return {}; }

    virtual int getPrimaryDimension()           { return 0; }
    virtual bool usesLineDepthFor(int dim)      { return false; }
    virtual float getDepth(int dim)             { return 1.f; }
    virtual float getValue(int dim)             { return 0; }

    virtual float getYellow()                   { return 0; }
    virtual float getRed()                      { return 0; }
    virtual float getBlue()                     { return 0; }

    virtual float getDistortedTime(int chan)    { return 0; }
};