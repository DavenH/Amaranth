#pragma once

#include <vector>
#include <iostream>

#include <Algo/Fir.h>
#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>
#include <UI/ColorGradient.h>
#include <UI/Panels/Panel.h>
#include "JuceHeader.h"

using std::cout;
using std::endl;
using std::vector;

class DerivativePanel :
        public Component
    , 	public Updateable
    , 	public SingletonAccessor {
public:
    explicit DerivativePanel(SingletonRepo* repo);

    void calcDerivative();
    void calcRasterDerivative();
    void mouseEnter(const MouseEvent& e) override;
    void paint(Graphics& g) override;
    void performUpdate(UpdateType updateType) override;

private:
    bool haveDrawn;
    size_t frameIndex;

    ColorGradient gradient;
    CriticalSection cSection;

    ScopedAlloc<Ipp32f> workMemory;
    ScopedAlloc<Ipp16s> indices;
    Buffer<float> exes;
    std::unique_ptr<FIR> fir;
};
