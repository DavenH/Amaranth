#pragma once

#include <Inter/Interactor2D.h>
#include <Inter/Interactor2D.h>
#include <Obj/Ref.h>

class Spectrum2D;
class Spectrum3D;

class SpectrumInter2D :
        public Interactor2D {
public:

    explicit SpectrumInter2D(SingletonRepo* repo);
    SpectrumInter2D(const SpectrumInter2D& SpectrumInter2D);

    void init() override;
    bool locateClosestElement() override;
    void showCoordinates() override;
    void doExtraMouseUp() override;
    bool isCurrentMeshActive() override;
    Interactor* getOppositeInteractor() override;

    int getClosestHarmonic() const { return closestHarmonic; }
    void setClosestHarmonic(int num) { closestHarmonic = num; }

private:
    int closestHarmonic;
    Ref <Spectrum3D> spectrum3D;
};
