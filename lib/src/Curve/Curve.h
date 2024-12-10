#pragma once
#include <cmath>
#include <vector>
#include <ippdefs.h>
#include <iostream>
#include "Vertex.h"
#include "Vertex2.h"
#include "Intercept.h"

using std::cout;
using std::endl;
using std::ostream;

class TransformParameters {
public:
	float theta;
	float scaleX;
	float scaleY;
	float shear;
	float sinrot;
	float cosrot;
	int dpole, ypole;
	Vertex2 d;
	friend ostream& operator<<(ostream& stream, TransformParameters t);
};

#ifndef _USE_IPP_AFFINE_TRANSFORM
	#define _USE_IPP_AFFINE_TRANSFORM 0
#endif

class Curve {
public:
	static const int resolutions 	= 3u;
	static const int resolution 	= 64u;
	static const int numCurvelets 	= 128u;

	Ipp32f transformX[resolution];
	Ipp32f transformY[resolution];

	static float*** table;

	Intercept a, b, c;
	TransformParameters tp;
	int resIndex;
	int curveRes;
	int waveIdx;

	Curve(const Intercept& one, Intercept two, Intercept thr);
	Curve(Vertex2& one, Vertex2& two, Vertex2& thr);
	Curve(const Curve& curve);
	~Curve();

    void nullify() {
		waveIdx 		= 0;
		tableCurveIdx 	= 0;
		tableCurvePos 	= 0;
		resIndex 		= 0;
		interpolate 	= false;
	}

	bool operator==(const Curve& other);
	double function(const double x, const double t);
	float getCentreX();
	static void calcTable();
	static void deleteTable();
	void construct();
	void destruct();
	void recalculateCurve();
	void recalculatedPadded();
	void setResIndex(int index);
	void setShouldInterpolate(bool should)  { interpolate = should; }
	void update();
	void updateCurrentIndex();
	void validate();

private:
	int tableCurveIdx;
	float tableCurvePos;
	bool interpolate;

	static inline float yValue(int theta, int index, int res);

	JUCE_LEAK_DETECTOR(Curve);
};