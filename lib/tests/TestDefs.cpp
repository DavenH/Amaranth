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
