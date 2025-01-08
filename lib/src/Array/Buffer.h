#pragma once
#include "JuceHeader.h"

using namespace juce;

#ifdef USE_IPP
    #include <ipp.h>
    #define perfSplit(X, Y) X
    #include <ipp.h>
    using Int8u = Int8u;
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
    explicit Buffer(AudioSampleBuffer& audioBuffer, int chan = 0);

    virtual ~Buffer() = default;

    /* —————————————————————————————————————————————————————————————————————————————————————————————————————— */

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
    T stddev()  const;
    T sum()     const;
    T normDiffL2(Buffer buff) const;  // sqrt(sum(buff[i] - ptr[i])**2)
    T dot(Buffer buff) const;

    // init (overwrites)
    Buffer& zero();
    Buffer& zero(int size);
    Buffer& rand(unsigned& seed);
    Buffer& ramp();                  // ptr[i] = i / (sz - 1)
    Buffer& ramp(T offset, T delta); // ptr[i] = offset + i * delta
    Buffer& hann();
    Buffer& blackman();

    // nullary math
    Buffer& abs();
    Buffer& inv(); // reciprocal
    Buffer& exp();
    Buffer& ln(); // natural log
    Buffer& sin();
    Buffer& tanh();
    Buffer& sqr();
    Buffer& sqrt();
    Buffer& undenormalize();

    // unary constant
    Buffer& set(T c);
    Buffer& pow(T c);
    Buffer& add(T c);
    Buffer& mul(T c);
    Buffer& div(T c);
    Buffer& sub(T c);

    // reverse ops
    Buffer& subCRev(T c); // ptr[i] = c - ptr[i]
    Buffer& divCRev(T c); // ptr[i] = c / ptr[i]
    Buffer& powCRev(T c); // ptr[i] = c ** ptr[i]
    Buffer& subCRev(T c, Buffer buff); // ptr[i] = c - buff[i]

    // unary buffer
    Buffer& sub(Buffer buff); // ptr[i] -= buff[i]
    Buffer& add(Buffer buff); // ptr[i] += buff[i]
    Buffer& mul(Buffer buff); // ptr[i] *= buff[i]
    Buffer& div(Buffer buff); // ptr[i] /= buff[i]

    Buffer& add(Buffer buff, T c); // ptr[i] += buff[i] * c
    Buffer& addProduct(Buffer buff, T c);
    Buffer& addProduct(Buffer src1, Buffer src2);

    // thresholding
    Buffer& clip(T low, T high);
    Buffer& threshLT(T c);
    Buffer& threshGT(T c);

    // value shifting
    Buffer& flip();
    Buffer& withPhase(int phase, Buffer workBuffer);
    Buffer& sort();

    // returns phase
    int downsampleFrom(Buffer buff, int factor = -1, int phase = 0);
    int upsampleFrom(Buffer buff, int factor = -1, int phase = 0);

    /* —————————————————————————————————————————————————————————————————————————————————————————————————————— */

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

    void mul(const Buffer<float> & c, float x);

protected:
    int sz;
    T* ptr;
};
