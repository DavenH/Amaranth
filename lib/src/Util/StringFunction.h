#pragma once
#include "JuceHeader.h"

using namespace juce;

namespace Ops {
    enum {
		Add
	,	Sub
	,	Mul
	,	Div
	,	Max
	,	Min
	,	Rnd
	,	Flr
	,	Ceil
	,	Abs
	,	Log
	,	Pow
	,	PowRev
	,	Np2
	};
}

class StringFunction {
public:
	struct Op {
		int code;
		double arg1;

		Op() : code(Ops::Add) {}
		Op(int code) : code(code) {}
		Op(int code, double arg1) : code(code), arg1(arg1) {}
		double eval(double arg);
	};

	StringFunction(int prec) : precision(prec) {}
	StringFunction() : precision(1) {}

    StringFunction& chain(int code, double value) {
        ops.add(Op(code, value));
        return *this;
    }

    StringFunction& chain(int code) {
        ops.add(Op(code));
        return *this;
    }

    StringFunction clone() {
        return *this;
    }

    StringFunction& withPrecision(int prec) {
        precision = prec;
        return *this;
    }

    double evaluate(int idx, double curr);

	StringFunction setPreString(const String& str)
	{ preString = str; return *this; }

    StringFunction withPostString(const String& str) const {
		StringFunction copy = *this;
		copy.postString = str;

		return copy;
	}

	String toString(double value);

private:
	String roundedString(double value);
	String paddedString(double value);

	int precision;
	String preString, postString;

	Array<Op> ops;
};