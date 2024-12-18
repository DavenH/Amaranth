#pragma once

class SynthFlag
{
public:
    bool haveVolume{};
    bool havePitch{};
    bool haveTime{};
    bool haveFFTPhase{};
    bool haveOscPhase{};
    bool haveFilter{};
    bool haveCutoff{};
    bool haveResonance{};
    bool fadingOut{};
    bool fadingIn{};
    bool playing{};
    bool looping{};
    bool latencyFilling{};
    bool releasing{};
    bool sustaining{};

    SynthFlag()	= default;
};
