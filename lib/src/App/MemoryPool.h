#pragma once

#include "../Array/Buffer.h"
#include "../Array/ScopedAlloc.h"
#include "SingletonAccessor.h"

class MemoryPool : public SingletonAccessor {
public:
    explicit MemoryPool(SingletonRepo* repo);

    ~MemoryPool() override = default;

    Buffer<float> getAudioPool() 	{ return audioPool; }
    Buffer<float> getMainPool() 	{ return mainPool; 	}
    Buffer<float> getGraphicsPool() { return gfxPool; 	}

private:
    ScopedAlloc<float> audioPool, gfxPool, mainPool;
};
