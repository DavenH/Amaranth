#pragma once

class TourGuide
{
public:
    virtual ~TourGuide() = default;

    virtual Component* getComponent(int) = 0;
};

