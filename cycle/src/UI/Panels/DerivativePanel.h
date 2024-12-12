#pragma once

#include <vector>
#include <iostream>

#include <Algo/Fir.h>
#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>
#include <Obj/ColorGradient.h>
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
	DerivativePanel(SingletonRepo* repo);

	void calcDerivative();
	void calcRasterDerivative();
	void mouseEnter(const MouseEvent& e);
	void paint(Graphics& g);
	void performUpdate(int updateType);

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
