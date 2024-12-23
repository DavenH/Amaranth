#pragma once
#include "JuceHeader.h"
using namespace juce;

template<class T>
class Buffer
{
public:
    Buffer() 								: ptr(0), 			sz(0) 			{}
    Buffer(T* array, int size) 				: ptr(array), 		sz(size) 		{}
    Buffer(const Buffer& buff) 				: ptr(buff.get()), 	sz(buff.size()) {}
    Buffer(const Buffer& buff, int size)	: ptr(buff.get()), 	sz(size) 		{ jassert(buff.size() >= size); }
    explicit Buffer(AudioSampleBuffer& audioBuffer, int chan = 0);

    virtual ~Buffer() = default;

    /* —————————————————————————————————————————————————————————————————————————————————————————————————————— */

    void copyTo(Buffer buff) 	 const;
    void getMax(T& pMax, int& index) const;
    void getMin(T& pMin, int& index) const;
    void minmax(T& pMin, T& pMax) 	 const;

    [[nodiscard]] bool isProbablyEmpty() const;

    int downsampleFrom(Buffer buff, int factor = -1, int phase = 0);
    int upsampleFrom(Buffer buff, int factor = -1, int phase = 0);

    T max() 	const;
    T mean()	const;
    T min() 	const;
    T normL1() 	const;
    T normL2() 	const;
    T stddev() 	const;
    T sum() 	const;

    T dot(Buffer buff) const;
    T normDiffL2(Buffer buff) const;

    Buffer& withPhase(int phase, Buffer workBuffer);
    Buffer& zero(int size);
    Buffer& set(T val);
    Buffer& abs();
    Buffer& exp();
    Buffer& inv();
    Buffer& ln();
    Buffer& sin();
    Buffer& sort();
    Buffer& sqr();
    Buffer& sqrt();
    Buffer& zero();
    Buffer& diff(Buffer buff);
    Buffer& pow(T val);
    Buffer& sub(T c);
    Buffer& sub(Buffer buff);
    Buffer& sub(Buffer src1, Buffer src2);
    Buffer& subCRev(T c);
    Buffer& subCRev(T c, Buffer buff);
    Buffer& add(T c);
    Buffer& add(Buffer buff, T c);
    Buffer& add(Buffer buff);
    Buffer& add(Buffer src1, Buffer src2);
    Buffer& mul(T c);
    Buffer& mul(Buffer buff);
    Buffer& mul(Buffer buff, T c);
    Buffer& mul(Buffer src1, Buffer src2);
    Buffer& div(Buffer buff);
    Buffer& div(T c);
    Buffer& divCRev(T c);
    Buffer& flip();
    Buffer& flip(Buffer buff);
    Buffer& rand(unsigned& seed);
    Buffer& ramp();
    Buffer& ramp(T offset, T delta);
    Buffer& threshLT(T c);
    Buffer& threshGT(T c);
    Buffer& addProduct(Buffer buff, T c);
    Buffer& addProduct(Buffer src1, Buffer src2);
    Buffer& conv(Buffer src1, Buffer src2, Buffer<unsigned char> workBuff);

    /* —————————————————————————————————————————————————————————————————————————————————————————————————————— */

    T* begin() 					{ return ptr; 		}
    T* end() 					{ return ptr + sz; 	}
    T& front() 					{ jassert(sz > 0); 	return ptr[0]; 		}
    T& back() 					{ jassert(sz > 0); 	return ptr[sz - 1];	}
    const T& front() const 		{ jassert(sz > 0); 	return ptr[0]; 		}
    const T& back() const		{ jassert(sz > 0);	return ptr[sz - 1]; }

    T* get() const				{ return ptr; 		}
    operator T*()				{ return ptr; 		}
    operator const T*() const 	{ return ptr; 		}

    [[nodiscard]] bool empty() const	{ return sz == 0; 	}
    [[nodiscard]] int size() const		{ return sz; 		}

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

    void operator+=(const Buffer& other); //	{ add(other);	}
    void operator+=(T val); //					{ add(val);		}
    void operator-=(const Buffer& other); //	{ sub(other);	}
    void operator-=(T val); // 					{ sub(val);		}
    void operator*=(const Buffer& other); //	{ mul(other);	}
    void operator*=(T val); //					{ mul(val);		}
    void operator/=(const Buffer& other); //	{ div(other);	}
    void operator/=(T val); //					{ div(val);		}

    void operator<<(const Buffer& other);
    // {
    //     other.copyTo(*this);
    // }

    void operator>>(Buffer other) const;
    // {
    //     copyTo(other);
    // }

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
