#ifndef CONVOLVER_H_
#define CONVOLVER_H_

#include "../App/SingletonAccessor.h"
#include "../Array/Buffer.h"
#include "../Array/ScopedAlloc.h"

class Convolver : public SingletonAccessor {
public:
	Convolver(SingletonRepo* repo);
	~Convolver() {}
	void setKernel(Buffer<float> kernel);

	Buffer<float> processBuffer(Buffer<float> buffer);
	Buffer<float> convolve(Buffer<float> kernelFrq, Buffer<float> inputFrq);

private:
	Buffer<float> kernel;
	ScopedAlloc<float> paddedKernel;
	ScopedAlloc<float> paddedInput;
};

#endif
