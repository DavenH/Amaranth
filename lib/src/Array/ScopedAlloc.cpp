#include "ScopedAlloc.h"
#include "ArrayDefs.h"
#include "VecOps.h"

#define constructScopedAlloc(T)          \
template<>                               \
ScopedAlloc<T>::ScopedAlloc(int size) :  \
        Buffer<T>(nullptr, size)         \
{                                        \
    jassert(size >= 0);                  \
    size         = jmax(0, size);        \
    Buffer::sz   = size;                 \
    Buffer::ptr  = VecOps::allocate<T>(size); \
    placementPos = 0;                    \
    alive        = true;                 \
}

#define resizeIpp(T)                     \
template<>                               \
bool ScopedAlloc<T>::resize(int size) {  \
    jassert(size >= 0);                  \
    size = jmax(0, size);                \
    if(Buffer::sz == size)               \
        return false;                    \
    clear();                             \
    Buffer::sz  = size;                  \
    Buffer::ptr = VecOps::allocate<T>(size); \
    alive       = true;                  \
    return true;                         \
}

defineForAllTypes(resizeIpp)
defineForAllTypes(constructScopedAlloc)
