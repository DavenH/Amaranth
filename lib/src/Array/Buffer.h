#pragma once
#include "JuceHeader.h"

using namespace juce;

  #ifdef USE_IPP
    #include <ipp.h>
    #define perfSplit(X, Y) X
    #include <ipp.h>
    using Int8u = Ipp8u;
    using Int8s = Ipp8s;
    using Int16s = Ipp16s;
    using Int32s = Ipp32s;
    using Float32 = Ipp32f;
    using Float64 = Ipp64f;
    using Complex32 = Ipp32fc;
    using Complex64 = Ipp64fc;
  #elif defined(USE_ACCELERATE)
    #define perfSplit(X, Y) Y
    #include <cstdint>
    #include <complex>

    using Int8u = uint8_t;
    using Int8s = int8_t;
    using Int16s = int16_t;
    using Int16u = uint16_t;
    using Int32s = int32_t;
    using Int32u = uint32_t;
    using Float32 = float;
    using Float64 = double;
    using Complex32 = std::complex<float>;
    using Complex64 = std::complex<double>;
  #else
    #error "No Performance Library specified"
  #endif

template<class T>
class Buffer
{
public:
    Buffer()                                : ptr(0),           sz(0)           {}
    Buffer(T* array, int size)              : ptr(array),       sz(size)        {}
    Buffer(const Buffer& buff)              : ptr(buff.get()),  sz(buff.size()) {}
    Buffer(const Buffer& buff, int size)    : ptr(buff.get()),  sz(size)        { jassert(buff.size() >= size); }
    explicit Buffer(AudioBuffer<T>& audioBuffer, int chan = 0);

    virtual ~Buffer() = default;

    /* ——————————————————————————————————————————————————————————————————————————————— */

    void copyTo(Buffer buff)         const;
    void getMax(T& pMax, int& index) const;
    void getMin(T& pMin, int& index) const;
    void minmax(T& pMin, T& pMax)    const;

    [[nodiscard]] bool isProbablyEmpty() const;

    // stats
    T max()     const;
    T mean()    const;
    T min()     const;
    T normL1()  const;                // sum(abs(ptr[i]))
    T normL2()  const;                // sqrt(sum(ptr[i])**2)
    T stddev()  const;                // standard deviation
    T sum()     const;                // sum(ptr[i])
    T normDiffL2(Buffer buff) const;  // sqrt(sum(buff[i] - ptr[i])**2)
    T dot(Buffer buff) const;

    // init (overwrites existing data)
    Buffer& zero();                  // ptr[i] = 0
    Buffer& zero(int size);          // ptr[i] = 0, 0<=i< size
    Buffer& rand(unsigned& seed);    // ptr[i] = rand(0, 1)
    Buffer& ramp();                  // ptr[i] = i / (sz - 1)
    Buffer& ramp(T offset, T delta); // ptr[i] = offset + i * delta
    Buffer& hann();                  // ptr[i] = winHann(i/(sz-1))
    Buffer& blackman();              // ptr[i] = winBlackman(i/(sz-1))
    Buffer& sin(float relFreq, float unitPhase = T()); // ptr[i] = sin(2 * pi * relFreq * i + 2 * pi * unitPhase)

    // the remaining methods operate on the contents of underlying array.
    // Expressions that totally replace the contents will be in VecOps,
    // and of the form op(src, args, dst)

    // nullary math
    Buffer& abs();  // ptr[i] = abs(ptr[i])
    Buffer& inv();  // ptr[i] = 1 / ptr[i]
    Buffer& exp();  // ptr[i] = e ** ptr[i]
    Buffer& ln();   // ptr[i] = log_e(ptr[i])
    Buffer& sin();  // ptr[i] = sin(ptr[i])
    Buffer& tanh(); // ptr[i] = tanh(ptr[i])
    Buffer& sqr();  // ptr[i] = ptr[i] ** 2
    Buffer& sqrt(); // ptr[i] = ptr[i] ** 0.5

    // unary constant
    Buffer& set(T c); // ptr[i] = c
    Buffer& pow(T c); // ptr[i] **= c
    Buffer& add(T c); // ptr[i] += c
    Buffer& mul(T c); // ptr[i] *= c
    Buffer& div(T c); // ptr[i] /= c, unless c = 0, then it is a no-op
    Buffer& sub(T c); // ptr[i] -= c

    // reverse ops
    Buffer& subCRev(T c); // ptr[i] = c - ptr[i]
    Buffer& divCRev(T c); // ptr[i] = c / ptr[i]
    Buffer& powCRev(T c); // ptr[i] = c ** ptr[i]

    // unary buffer
    Buffer& sub(Buffer buff); // ptr[i] -= buff[i]
    Buffer& add(Buffer buff); // ptr[i] += buff[i]
    Buffer& mul(Buffer buff); // ptr[i] *= buff[i]
    Buffer& div(Buffer buff); // ptr[i] /= buff[i]

    Buffer& addProduct(Buffer buff, T c); // ptr[i] += buff[i] * c
    Buffer& addProduct(Buffer src1, Buffer src2); // ptr[i] += src1[i] + src2[i]

    // thresholding
    Buffer& clip(T low, T high); // ptr[i] = max(low, min(high, ptr[i]))
    Buffer& threshLT(T c); // ptr[i] = max(c, ptr[i])
    Buffer& threshGT(T c); // ptr[i] = min(c, ptr[i])

    // value shifting
    Buffer& flip(); // ptr[i] = ptr[sz-1-i]
    Buffer& withPhase(int phase, Buffer workBuffer); // ptr[(i + phase)%sz] = ptr[i]
    Buffer& sort(); // sorts in ascending order

    // returns phase
    int downsampleFrom(Buffer buff, int factor = -1, int phase = 0);
    int upsampleFrom(Buffer buff, int factor = -1, int phase = 0);

    /* ——————————————————————————————————————————————————————————————————————————————— */

    T* begin()                  { return ptr;       }
    T* end()                    { return ptr + sz;  }
    T& front()                  { jassert(sz > 0);  return ptr[0];      }
    T& back()                   { jassert(sz > 0);  return ptr[sz - 1]; }
    const T& front() const      { jassert(sz > 0);  return ptr[0];      }
    const T& back() const       { jassert(sz > 0);  return ptr[sz - 1]; }

    T* get() const              { return ptr;       }
    operator T*()               { return ptr;       }
    operator const T*() const   { return ptr;       }

    [[nodiscard]] bool empty() const    { return sz == 0;   }
    [[nodiscard]] int size() const      { return sz;        }

    void nullify() {
        ptr = 0;
        sz = 0;
    }

    T& operator[](const int i) {
        jassert(i >= 0 && i < sz);
        return ptr[i];
    }

    T const& operator[](const int i) const {
        jassert(i >= 0 && i < sz);
        return ptr[i];
    }

    Buffer withSize(int newSize) const {
        jassert(newSize <= sz);
        return Buffer(ptr, newSize);
    }

    Buffer offset(int num) const {
        jassert(num >= 0 && num < sz);
        return Buffer(ptr + num, sz - num);
    }

    Buffer section(int start, int size) const {
        jassert(sz >= size + start);
        jassert(size > 0);

        return Buffer(ptr + start, size);
    }

    Buffer sectionAtMost(int start, int size) const {
        return Buffer(ptr + start, jmax(0, jmin(sz - start, size)));
    }

    Buffer operator+(const int num) const {
        jassert(num >= 0 && num < sz);

        return Buffer(ptr + num, sz - num);
    }

    Buffer operator-(const int num) const {
        return Buffer(ptr - num, sz + num);
    }

    void operator+=(const Buffer& other);
    void operator+=(T val);
    void operator-=(const Buffer& other);
    void operator-=(T val);
    void operator*=(const Buffer& other);
    void operator*=(T val);
    void operator/=(const Buffer& other);
    void operator/=(T val);

    void operator<<(const Buffer& other);
    void operator>>(Buffer other) const;

    Buffer& operator=(const Buffer& other) = default;

    virtual bool resize(int newSize) { sz = newSize; return true; }

    template<class S>
    Buffer<S> toType() const {
        int newSize = (sizeof(T) * sz) / sizeof(S);
        return Buffer<S>(reinterpret_cast<S*>(ptr), newSize);
    }

protected:
    int sz;
    T* ptr;
};
