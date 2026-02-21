#include "StringFunction.h"
#include "../Util/NumberUtils.h"

String StringFunction::paddedString(double value) const {
    return preString + roundedString(value) + postString;
}

String StringFunction::roundedString(double value) const {
    return precision == 0 ?
           String(int(value)) :
           String(value, precision);
}

double StringFunction::evaluate(int idx, double curr) {
    if (idx == 0) {
        return curr;
}

    return evaluate(idx - 1, ops[idx].eval(curr));
}

String StringFunction::toString(double value) {
    return paddedString(evaluate(ops.size() - 1, value));
}

double StringFunction::Op::eval(double arg) const {
    using namespace Ops;

    switch (code) {
        case Add:   return arg + arg1;
        case Sub:   return arg - arg1;
        case Mul:   return arg * arg1;
        case Div:   return arg / arg1;
        case Max:   return jmax(arg, arg1);
        case Min:   return jmin(arg, arg1);
        case Rnd:   return roundToInt(arg);
        case Flr:   return int(arg);
        case Ceil:  return int(arg + 1 - 1e-9f);
        case Abs:   return std::abs(arg);
        case Pow:   return powf(arg, arg1);
        case Log:   return logf(arg);
        case PowRev: return powf(arg1, arg);
        case Np2:   return 2 << static_cast<int>(ceilf(logf(arg)));
        default: break;
    }

    throw std::invalid_argument("Invalid operator " + String(code).toStdString());
}
