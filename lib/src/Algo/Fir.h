#ifndef _FIR_H_
#define _FIR_H_

#include <ipp.h>
#include "../Array/Buffer.h"
#include "../Array/ScopedAlloc.h"
#include "../Obj/CurveLine.h"
#include "JuceHeader.h"

class FIR {
public:
    FIR() : firState(nullptr) {
    }

    FIR(double rateRatio, int tapsSize = 32, bool compensateLatency = true) {
        this->tapsSize = tapsSize;

        jassert(tapsSize >= 5 && tapsSize <= 64);

        mem.resize(tapsSize * 5 / 2);

        delayLine   = mem.place(tapsSize);
        taps32      = mem.place(tapsSize);
        moveBuffer  = mem.place(tapsSize / 2);

        rateRatio = jmin(0.499999, rateRatio * 0.5);

        {
            int workSize = 0;
            ippsFIRGenGetBufferSize(tapsSize, &workSize);
            ScopedAlloc<Ipp8u> workBuff(workSize);
            ScopedAlloc<Ipp64f> taps64(tapsSize);

            ippsFIRGenLowpass_64f(rateRatio, taps64, tapsSize, ippWinBlackman, ippTrue, workBuff);
            ippsConvert_64f32f(taps64, taps32, tapsSize);
        }

        init(compensateLatency);
    }

    FIR(Buffer<float> taps, bool compensateLatency = true) {
        tapsSize = taps.size();
        mem.resize(tapsSize * 5 / 2);

        delayLine   = mem.place(tapsSize);
        taps32      = mem.place(tapsSize);
        moveBuffer  = mem.place(tapsSize / 2);

        init(compensateLatency);
    }

    void reset() {
        delayLine.zero();
        ippsFIRSR_32f(moveBuffer, moveBuffer, moveBuffer.size(), firState, delayLine, delayLine, workBuff);
    }

    void process(Buffer<float> buff) {
        if (firState != nullptr && !buff.empty()) {
            int padSize = moveBuffer.size();
            if (compensateLatency) {
                if (padSize < buff.size()) {
//                  Line<float> line(Point<float>(0, buff.front()), Point<float>(padSize, buff[padSize]));

                    CurveLine line(Vertex2(0, buff.front()),
                                   Vertex2(padSize, buff[padSize]));

                    moveBuffer.ramp(line.at(-padSize), line.slope);
                    delayLine.set(moveBuffer.front());
                    ippsFIRSR_32f(moveBuffer, moveBuffer, padSize, firState, delayLine, delayLine, workBuff);

                    // prepare tail before buff is changed
                    line.setPoints(Vertex2(-padSize, buff[buff.size() - 1 - padSize]),
                                   Vertex2(0, buff.back()));

                    moveBuffer.ramp(line.at(0), line.slope);
                } else {
                    delayLine.set(buff.front());
                }
            }

            ippsFIRSR_32f(buff, buff, buff.size(), firState, delayLine, delayLine, workBuff);

            if (compensateLatency) {
                int zeroSize = jmin(padSize, buff.size());
                int moveSize = buff.size() - zeroSize;

                if (moveSize > 0) {
//                  ippsFIRSetDlyLine_32f(firState, delayLine);
                    ippsFIRSR_32f(moveBuffer, moveBuffer, zeroSize, firState, delayLine, delayLine, workBuff);
                    ippsMove_32f(buff + zeroSize, buff, moveSize);
                    moveBuffer.copyTo(buff + moveSize);
                }
            }
        }
    }

    void resetAndProcess(const Buffer<float>& buff) {
        reset();
        process(buff);
    }

    ~FIR() {
        if (firState != nullptr) {
            firState = nullptr;
        }
    }

private:
    bool compensateLatency;
    int tapsSize;

    Buffer<float> moveBuffer;
    Buffer<float> delayLine;
    Buffer<float> taps32;
    ScopedAlloc<Ipp8u> workBuff;
    ScopedAlloc<Ipp8u> stateBuff;
    ScopedAlloc<float> mem;

    IppsFIRSpec_32f* firState;

    void init(bool compensate) {
        this->compensateLatency = compensate;
        delayLine.zero();
        int buffSize, specSize;
        ippsFIRSRGetSize(taps32.size(), ipp32f, &specSize, &buffSize);
        stateBuff.ensureSize(specSize);
        workBuff.ensureSize(buffSize);

        firState = (IppsFIRSpec_32f*) stateBuff.get();
        ippsFIRSRInit_32f(taps32, taps32.size(), IppAlgType::ippAlgAuto, firState);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FIR);
};


#endif
