#ifndef _wave2Dinteractor_h
#define _wave2Dinteractor_h

#include <Inter/Interactor2D.h>

class WaveformInter2D :
	public Interactor2D
{
public:
	WaveformInter2D(SingletonRepo* mgr);
	bool isCurrentMeshActive();

	void showCoordinates();
	Interactor* getOppositeInteractor();
	void modelAudioCycle();

private:
	friend class Waveform2D;

};

#endif
