#pragma once

#include "../Curve/Vertex.h"

class MorphPosition {
public:
	float time, red, blue;
	float timeDepth, redDepth, blueDepth;

	MorphPosition()
			: time(0.f), red(0.f), blue(0.f), timeDepth(0.f), redDepth(0.f), blueDepth(0.f) {
    }

	MorphPosition(float t, float r, float b)
			: time(t), red(r), blue(b), timeDepth(1.f), redDepth(1.f), blueDepth(1.f) {
	}

	MorphPosition withTime(float t) {
		return MorphPosition(t, red, blue);
	}

	float timeEnd() const { return jmin(1.f, time + timeDepth); }
	float redEnd()  const { return jmin(1.f, red  + redDepth ); }
	float blueEnd() const { return jmin(1.f, blue + blueDepth); }

	void resetTime() 	  { time = 0; }

    static void getOtherDims(int dim, int& dimX, int& dimY) {
        switch (dim) {
		case Vertex::Time:
			dimX = Vertex::Red;
			dimY = Vertex::Blue;

			break;

		case Vertex::Red:
			dimX = Vertex::Time;
			dimY = Vertex::Blue;

			break;

		case Vertex::Blue:
			dimX = Vertex::Red;
			dimY = Vertex::Time;
			break;
		default:
			break;
        }
	}

    void getOtherPos(int fromDim, float& val1, float& val2) const {
        int dimX, dimY;
		getOtherDims(fromDim, dimX, dimY);

		val1 = operator [](dimX);
		val2 = operator [](dimY);
	}

    void getOtherDepths(int fromDim, float& val1, float& val2) const {
        int dimX, dimY;
		getOtherDims(fromDim, dimX, dimY);

		val1 = depthAt(dimX);
		val2 = depthAt(dimY);
	}

    float distanceTo(const MorphPosition& pos) const {
		float dist = 0;

		dist += (time - pos.time) * (time - pos.time);
		dist += (red  - pos.red)  * (red  - pos.red);
		dist += (blue - pos.blue) * (blue - pos.blue);

		return sqrtf(dist);
	}

    float operator[](const int index) const {
        switch (index) {
			case Vertex::Time: 	return time;
			case Vertex::Red: 	return red;
			case Vertex::Blue: 	return blue;

			default:
				jassertfalse;
		}

		return time;
	}

    float& operator[](const int index) {
        switch (index) {
			case Vertex::Time: 	return time;
			case Vertex::Red: 	return red;
			case Vertex::Blue: 	return blue;
		}

		return time;
	}

    float depthAt(const int index) const {
        switch (index) {
			case Vertex::Time: 	return timeDepth;
			case Vertex::Red: 	return redDepth;
			case Vertex::Blue: 	return blueDepth;
		}

		return time;
	}

    float& depthAt(const int index) {
        switch (index) {
			case Vertex::Time: 	return timeDepth;
			case Vertex::Red: 	return redDepth;
			case Vertex::Blue: 	return blueDepth;
		}

		return time;
	}

	Rectangle<float> toRect(int dim) const {
		float x, y, w, h;
		getOtherPos(dim, x, y);
		getOtherDepths(dim, w, h);

		return Rectangle<float>(x, y, w, h);
	}
};
