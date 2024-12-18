#include "ScopedAlloc.h"

#define constructScopedAlloc(T) 				\
template<>										\
ScopedAlloc<Ipp##T>::ScopedAlloc(int size) :	\
        Buffer<Ipp##T>(nullptr, size)			\
{												\
    Buffer::ptr 	= ippsMalloc_##T(size);		\
    placementPos 	= 0;						\
    alive 			= true;						\
}


#define resizeIpp(T) 							\
template<> 										\
bool ScopedAlloc<Ipp##T>::resize(int size) { 	\
    if(Buffer::sz == size)  					\
        return false; 							\
    clear();									\
    Buffer::sz 	= size;							\
    Buffer::ptr = ippsMalloc_##T(size);			\
    alive 		= true;							\
    return true;								\
}

declareForAllTypes(resizeIpp)
declareForAllTypes(constructScopedAlloc)

///*
template<>
ScopedAlloc<Ipp32f>::ScopedAlloc(int size) :
    Buffer(nullptr, size)
{
    placementPos = 0;
    ptr  = ippsMalloc_32f(size);
    alive 		 = true;
}

template<>
bool ScopedAlloc<Ipp32f>::resize(int size)
{
    if(sz == size)
    {
        return false;
    }

    clear();
    sz 		= size;
    ptr 	= ippsMalloc_32f(size);
    alive 	= true;
    return true;
}
//*/
