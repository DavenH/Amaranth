#pragma once

#include "JuceHeader.h"
#include <ipp.h>

class VertCube;

using namespace juce;

class Vertex {
public:
    enum {
		Time, Phase, Amp, Red, Blue, Curve,
		numElements
	};

    Vertex() {
		initValues();
	}

	Vertex(float x, float y) {
		initValues();
		values[Phase] = x;
		values[Amp]   = y;
	}

	Vertex(float x, float y, float z, float r, float b) {
		initValues();
		values[Time] = x;
		values[Phase] = y;
		values[Amp] = z;
		values[Red] = r;
		values[Blue] = b;
	}

	~Vertex() = default;

	void addOwner(VertCube* cube);

	void removeOwner(VertCube* cube) 			{ owners.removeFirstMatchingValue(cube); }
	void setMaxSharpness() 						{ values[Curve] = 1.f;			}
	bool isOwnedBy(VertCube* cube) const		{ return owners.contains(cube); }
	bool unattached() const 					{ return owners.size() == 0; 	}
	int getNumOwners() const 					{ return owners.size(); 		}

	float constGetElement(int index) const 		{ return values[index]; 		}
	bool operator<(const Vertex& other)			{ return values[Vertex::Phase] < other.values[Vertex::Phase]; }
	inline float& operator[](const int index) 	{ return values[index]; 		}

	float distSqr(const Vertex& other) {
		float dist = 0;

		for (int i = 0; i <= Vertex::Blue; ++i) {
			float diff = values[i] - other.values[i];
			dist += diff;
		}

		return dist;
	}

    Vertex& operator=(const Vertex& other) {
        ippsCopy_32f(other.values, values, numElements);
        owners = other.owners;

        return *this;
    }

    Vertex(const Vertex& copy) {
        ippsCopy_32f(copy.values, values, numElements);
		owners = copy.owners;
	}

    Vertex operator*(const float factor) {
		if(factor == 1)
			return *this;

		if(factor == 0)
			return {};

		Vertex vertex;
		for (int i = 0; i < numElements; ++i)
			vertex.values[i] = values[i] * factor;

		return vertex;
	}

    void operator*=(const float factor) {
        if (factor == 1)
            return;

        if (factor == 0) {
            ippsZero_32f(values, numElements);
			return;
		}

		for (float& value : values)
			value *= factor;
	}

    Vertex operator+(const Vertex& other) const {
        Vertex vertex;
        for (int i = 0; i < numElements; ++i)
            vertex.values[i] = values[i] + other.values[i];

        return vertex;
    }

    Vertex operator-(const Vertex& other) const {
        Vertex vertex;
        for (int i = 0; i < numElements; ++i)
            vertex.values[i] = values[i] - other.values[i];

		return vertex;
	}

    bool operator==(const Vertex& other) const {
        return values[0] == other.values[0] && values[1] == other.values[1] && values[2] == other.values[2]
               && values[3] == other.values[3]
               && values[4] == other.values[4];
    }

    bool isSameMorphPositionTo(Vertex* vert) const {
		return  values[Time] == vert->values[Time] &&
				values[Red]  == vert->values[Red] &&
				values[Blue] == vert->values[Blue];
	}

	void validate() const {
		jassert(values[Phase] >= 0.f);
		jassert(values[Time] >= 0.f 	&& values[Time] <= 1.f);
		jassert(values[Amp] >= 0.f 		&& values[Amp] <= 1.f);
		jassert(values[Red] >= 0.f 		&& values[Red] <= 1.f);
		jassert(values[Blue] >= 0.f 	&& values[Blue] <= 1.f);
	}

	Array<VertCube*> owners;
	float values[numElements]{};

private:

    void initValues() {
        ippsZero_32f(values, numElements);
		values[Amp] = 0.5f;
	}
};
