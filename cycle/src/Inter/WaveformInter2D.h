#pragma once

#include <Inter/Interactor2D.h>

class WaveformInter2D :
    public Interactor2D
{
public:
    explicit WaveformInter2D(SingletonRepo* mgr);
    bool isCurrentMeshActive() override;

    void showCoordinates() override;
    Interactor* getOppositeInteractor() override;
    void modelAudioCycle();

private:
    friend class Waveform2D;

};