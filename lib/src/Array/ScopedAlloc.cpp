#include "ScopedAlloc.h"
#include "ArrayDefs.h"
#include "VecOps.h"

#define constructScopedAlloc(T)          \
template<>                               \
ScopedAlloc<T>::ScopedAlloc(int size) :  \
        Buffer<T>(nullptr, size)         \
{                                        \
    Buffer::ptr  = VecOps::allocate<T>(size); \
    placementPos = 0;                    \
    alive        = true;                 \
}

#define resizeIpp(T)                     \
template<>                               \
bool ScopedAlloc<T>::resize(int size) {  \
    if (Buffer::sz == size)               \
        return false;                    \
    clear();                             \
    Buffer::sz  = size;                  \
    Buffer::ptr = VecOps::allocate<T>(size); \
    alive       = true;                  \
    return true;                         \
}

defineForAllTypes(resizeIpp)
defineForAllTypes(constructScopedAlloc)
