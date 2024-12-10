#ifndef _f1dinteractor_h
#define _f1dinteractor_h

#include <Inter/Interactor2D.h>
#include <Inter/Interactor2D.h>
#include <Obj/Ref.h>

class Spectrum2D;
class Spectrum3D;

class SpectrumInter2D :
        public Interactor2D {
public:

    SpectrumInter2D(SingletonRepo* repo);
    SpectrumInter2D(const SpectrumInter2D& SpectrumInter2D);

    void init();
    bool locateClosestElement();
    void showCoordinates();
    void doExtraMouseUp();
    bool isCurrentMeshActive();
    Interactor* getOppositeInteractor();

    int getClosestHarmonic() { return closestHarmonic; }
    void setClosestHarmonic(int num) { closestHarmonic = num; }

private:
    int closestHarmonic;
    Ref <Spectrum3D> spectrum3D;
};

#endif
