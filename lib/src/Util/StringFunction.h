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
		double arg1{};

		Op() : code(Ops::Add) {}
		Op(int code) : code(code) {}
		Op(int code, double arg1) : code(code), arg1(arg1) {}
		double eval(double arg);
	};

	explicit StringFunction(int prec) : precision(prec) {}
	StringFunction() : precision(1) {}

	StringFunction& mul(double value) {
		ops.add(Op(Ops::Mul, value));
		return *this;
	}
	StringFunction& div(double value) {
		ops.add(Op(Ops::Div, value));
		return *this;
	}
	StringFunction& add(double value) {
		ops.add(Op(Ops::Add, value));
		return *this;
	}
	StringFunction& sub(double value) {
		ops.add(Op(Ops::Sub, value));
		return *this;
	}
	StringFunction& max(double value) {
		ops.add(Op(Ops::Max, value));
		return *this;
	}
	StringFunction& min(double value) {
		ops.add(Op(Ops::Min, value));
		return *this;
	}
	StringFunction& rnd(double value) {
		ops.add(Op(Ops::Rnd, value));
		return *this;
	}
	StringFunction& flr() {
		ops.add(Op(Ops::Flr));
		return *this;
	}
	StringFunction& ceil() {
		ops.add(Op(Ops::Ceil));
		return *this;
	}
	StringFunction& abs(double value) {
		ops.add(Op(Ops::Abs, value));
		return *this;
	}
	StringFunction& log(double value) {
		ops.add(Op(Ops::Log, value));
		return *this;
	}
	StringFunction& pow(double value) {
		ops.add(Op(Ops::Pow, value));
		return *this;
	}
	StringFunction& powRev(double value) {
		ops.add(Op(Ops::PowRev, value));
		return *this;
	}
	StringFunction& np2(double value) {
		ops.add(Op(Ops::Np2, value));
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

    [[nodiscard]] StringFunction withPostString(const String& str) const {
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