#pragma once
#include "../Algo/FFT.h"
#include "../App/SingletonAccessor.h"
#include "JuceHeader.h"
#include "../Util/NumberUtils.h"

class Transforms : public SingletonAccessor {
public:
    /*
     * A cache of FFT tranform contexts for each of 12, 2^x window sizes, from 32 to 16384
     */
	explicit Transforms(SingletonRepo* repo);
	~Transforms() override;

	void init() override;

    Transform& chooseFFT(int powerOfTwo) {
		jassert(! (powerOfTwo & (powerOfTwo - 1)));
		jassert(powerOfTwo >= 32 && powerOfTwo <= 16384);

		int index = NumberUtils::log2i(powerOfTwo) - 3;

		return *ffts[index];
	}

private:
	std::unique_ptr<Transform> ffts[12];
};
