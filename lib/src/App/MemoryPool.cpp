#include "MemoryPool.h"
#include "../App/SingletonRepo.h"

MemoryPool::MemoryPool(SingletonRepo* repo) :
        SingletonAccessor(repo, "MemoryPool")
    ,	audioPool	(16384)
    , 	mainPool	(16384)
    , 	gfxPool		(16384) {
}

