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
    Buffer::ptr  = size > 0 ? VecOps::allocate<T>(size) : nullptr; \
    placementPos = 0;                    \
    alive        = Buffer::ptr != nullptr; \
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
    Buffer::ptr = size > 0 ? VecOps::allocate<T>(size) : nullptr; \
    alive       = Buffer::ptr != nullptr; \
    return true;                         \
}

defineForAllTypes(resizeIpp)
defineForAllTypes(constructScopedAlloc)
