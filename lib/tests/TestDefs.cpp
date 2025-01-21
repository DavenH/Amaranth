#include "TestDefs.h"

#ifdef USE_ACCELERATE
  float real(const Complex32& c) { return c.real(); }
  float imag(const Complex32& c) { return c.imag(); }
  float mag(const Complex32& c) { return std::abs(c); }
  Complex32 makeComplex(float r, float i) { return std::complex<float>(r, i); }
#elif defined(USE_IPP)
  float real(const Complex32& c) { return c.re; }
  float imag(const Complex32& c) { return c.im; }
  float mag(const Complex32& c) { return std::sqrt(c.re*c.re + c.im*c.im); }
  Complex32 makeComplex(float r, float i) { return Ipp32fc{r, i}; }
#endif


void print(const Buffer<Complex32>& buffer) {
    std::cout << std::fixed << std::setprecision(3);
    for (int i = 0; i < buffer.size(); ++i) {
      std::cout << i << "\t" << real(buffer[i]) << "\t" << imag(buffer[i]) << std::endl;
    }
  }

void print(const Buffer<Float32>& buffer) {
    std::cout << std::fixed << std::setprecision(3);
    for (int i = 0; i < buffer.size(); ++i) {
      std::cout << i << "\t" << buffer[i] << std::endl;
    }
  }
