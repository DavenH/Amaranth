#pragma once

#include <vector>
#include "../App/SingletonAccessor.h"
#include "../Array/Column.h"
#include "../Array/ScopedAlloc.h"

using std::vector;

class PitchedSample;
class IterableBuffer;

class ShortTimeFT {
public:
};

class Spectrogram : public SingletonAccessor {
public:
    enum {
        CalcPhases		= 1
    ,	UnwrapPhases	= 2
    ,	SaveTime		= 4
    ,	LogScale		= 8
    };

    explicit Spectrogram(SingletonRepo* repo);
    ~Spectrogram() override;

    void calculate(IterableBuffer*, int flags = 0);
    void unwrapPhaseColumns();

    vector<Column>& getMagColumns()   { return magColumns; }
    vector<Column>& getPhaseColumns() { return phaseColumns; }
    vector<Column>& getTimeColumns()  { return timeColumns; }

private:
    vector<Column> 		magColumns, phaseColumns, 	timeColumns;
    ScopedAlloc<float> 	magMemory, 	phaseMemory, 	timeMemory;
};