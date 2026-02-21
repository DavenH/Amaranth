#pragma once

#include "JuceHeader.h"

using namespace juce;

class Real {
public:
    Real() : val(0.f) {}
    Real(float val) : val(val) {}
    virtual ~Real() {}

    Real& div(float dividand) {
        if (dividand == 0.f) {
            jassertfalse;
            val = 0.f;
            return *this;
        }

        val = val / dividand;
        return *this;
    }

    Real& rdiv(float toDivide) {
        if (val == 0.f) {
            jassertfalse;
            return *this;
        }

        val = toDivide / val;
        return *this;
    }

    Real& ln() {
        static float invLog2 = 1.f / logf(2.f);
        if (val == 0.f) {
            jassertfalse;
            return *this;
        }

        val = logf(val) * invLog2;
        return *this;
    }

    Real& inv() {
        if (val == 0.f) {
            jassertfalse;
            return *this;
        }

        val = 1.f / val;
        return *this;
    }

    Real operator^(const Real& r) const {
        if (r.val == 0.f) {
            return Real(1.f);
}

        return Real(powf(val, r.val));
    }

    Real operator/(const Real& r) const {
        if (r.val == 0.f) {
            jassertfalse;
            return Real(val);
        }

        return Real(val / r.val);
    }

    /* ----------------------------------------------------------------------------- */

    float get() { return val; }
    Real& set(float v) {
        val = v;
        return *this;
    }

    // inplace
    Real& ithreshLT(float thres) {
        val = jmax(thres, val);
        return *this;
    }
    Real& ithreshGT(float thres) {
        val = jmin(thres, val);
        return *this;
    }

    Real& icub() {
        val = val * val * val;
        return *this;
    }
    Real& isqr() {
        val = val * val;
        return *this;
    }
    Real& ineg() {
        val = -val;
        return *this;
    }
    Real& isqrt() {
        val = sqrtf(val);
        return *this;
    }
    Real& iabs() {
        val = fabsf(val);
        return *this;
    }
    Real& iexp() {
        val = expf(val);
        return *this;
    }

    Real& isub(float s) {
        val -= s;
        return *this;
    }
    Real& iadd(float a) {
        val += a;
        return *this;
    }
    Real& imul(float m) {
        val *= m;
        return *this;
    }
    Real& ipow(float p) {
        val = powf(val, p);
        return *this;
    }
    Real& imod(float m) {
        val = fmodf(val, m);
        return *this;
    }

    Real threshLT(float thres) { return Real(jmax(thres, val)); }
    Real threshGT(float thres) { return Real(jmin(thres, val)); }

    Real cub() { return Real(val * val * val); }
    Real sqr() { return Real(val * val); }
    Real sqrt() { return Real(sqrtf(val)); }
    Real neg() { return Real(-val); }
    Real abs() { return Real(fabsf(val)); }
    Real exp() { return Real(expf(val)); }

    Real sub(float s) { return Real(val - s); }
    Real add(float a) { return Real(val + a); }
    Real mul(float m) { return Real(val * m); }
    Real pow(float p) { return Real(powf(val, p)); }
    Real mod(float m) { return Real(fmodf(val, m)); }

    Real& operator=(const int& v) {
        val = static_cast<float>(v);
        return *this;
    }
    Real& operator=(const float& v) {
        val = v;
        return *this;
    }
    Real& operator=(const Real& r) {
        val = r.val;
        return *this;
    }

    Real operator-(const Real& r) const { return Real(val - r.val); }
    Real operator+(const Real& r) const { return Real(val + r.val); }
    Real operator*(const Real& r) const { return Real(val * r.val); }

    void operator*=(const Real& r) { val = mul(r.val); }
    void operator/=(const Real& r) { val = div(r.val); }
    void operator-=(const Real& r) { val = sub(r.val); }
    void operator+=(const Real& r) { val = add(r.val); }
    void operator^=(const Real& r) { val = pow(r.val); }

    bool operator<(const Real& r) const { return val < r.val; }
    bool operator>(const Real& r) const { return val > r.val; }
    bool operator>=(const Real& r) const { return val >= r.val; }
    bool operator<=(const Real& r) const { return val <= r.val; }

    operator float() const { return val; }

private:
    float val;
};
