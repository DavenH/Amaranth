#include "Arithmetic.h"
#include "../App/SingletonRepo.h"
#include "../Curve/Vertex2.h"
#include <ippdefs.h>

void Arithmetic::applyLogMapping(Buffer<Ipp32f> array, float tension) {
    float iln = 1 / logf(IPP_2PI * tension + 1.000001f);

    array.mul(tension * IPP_2PI).add(1.000001).ln().mul(iln);
}

void Arithmetic::applyInvLogMapping(Buffer<Ipp32f> array, float tension) {
    float lntens = logf(IPP_2PI * tension + 1.000001f);

    array.mul(lntens).exp().add(-1.000001).mul(IPP_RPI * 0.5f / float(tension));
}

int Arithmetic::getNoteForValue(float value, const Range<int>& range) {
    return range.getStart() + range.getLength() * value;
}

float Arithmetic::getUnitValueForNote(int key, const Range<int>& range) {
    return (key - range.getStart()) / float(range.getLength());
}

int Arithmetic::getGraphicNoteForValue(float value, const Range<int>& range) {
    int noteValue = getNoteForValue(value, range);
    NumberUtils::constrain(noteValue, range);

    return noteValue;
}

float Arithmetic::getUnitValueForGraphicNote(int key, const Range<int>& range) {
    float value = getUnitValueForNote(key, range);
    NumberUtils::constrain(value, 0.f, 1.f);

    return value;
}

int Arithmetic::getNextPow2(float fperiod) {
    static const float invLog2 = 1.f / logf(2.f);

    return 1 << (int) ceilf(logf(fperiod) * invLog2);
}

float Arithmetic::logMapping(float tension, float x, bool useOffset) {
    float leftOffset = useOffset ? (powf(tension + 1, 0.05f) - 1) / float(tension) : 0;
    return logf(tension * (x + leftOffset) + 1) / logf(tension + 1);
}

float Arithmetic::invLogMapping(float tension, float x, bool useOffset) {
    float leftOffset = useOffset ? (powf(tension + 1, 0.05f) - 1) / float(tension) : 0;
    return (expf(x * logf(tension + 1)) - 1) / tension - leftOffset;
}

float Arithmetic::calcAdditiveScaling(int numHarmonics) {
    return 0.05f * powf(numHarmonics, -0.2f);
}

float Arithmetic::getRelativePan(float pan, float modPan) {
    return sqrtf(jmax(0.f, 1.f - fabsf(pan - modPan)));
}

void Arithmetic::getPans(float pan, float& left, float& right) {
    left = jmin(1.f, (float) 2 * (1 - pan));
    right = jmin(1.f, (float) 2 * pan);
}

void Arithmetic::unpolarize(Buffer<float> x) {
    x.mul(0.5f).add(0.5f);
}

void Arithmetic::polarize(Buffer<float> x) {
    x.mul(2.f).add(-1.f);
}

void Arithmetic::getInputOutputRates(int& inRate, int& outRate, double sampleRateReal) {
    int sampleRate = (int) (sampleRateReal + 0.0001);

    switch (sampleRate) {
        case 192000:	inRate 	= 147;		outRate = 640;		break;
        case 176400:	inRate 	= 1;		outRate = 4;		break;
        case 96000:		inRate 	= 147;		outRate = 320;		break;
        case 88200:		inRate 	= 1;		outRate = 2;		break;
        case 44100:		inRate 	= 1;		outRate = 1;		break;
        case 48000:		inRate 	= 147;		outRate = 160;		break;
        case 32000:		inRate 	= 441;		outRate = 320;		break;
        case 22050:		inRate 	= 2;		outRate = 1;		break;
        case 11025:		inRate 	= 4;		outRate = 1;		break;
        default:		inRate 	= 1;		outRate = 1;
    }
}

