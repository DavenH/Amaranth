#include <ipps.h>
#include <Algo/ConvReverb.h>
#include <App/SingletonRepo.h>
#include <Util/Arithmetic.h>

#include "Reverb.h"

#include "../../UI/Effects/GuilessEffect.h"
#include "../../UI/Effects/ReverbUI.h"

ReverbEffect::ReverbEffect(SingletonRepo* repo) :
		Effect			(repo, "ReverbEffect")
	,	leftConv		(repo)
	,	rightConv		(repo)
	,	phaseNoise		(0.1f)
	,	magnNoise		(0.1f)
	,	outBuffer		(2)
	,	kernel			(2)
	,	wetLevel		(0.5f)
	,	roomSize		(0.2f)
	,	highpass		(0.05f)
	,	perBlockDecay	(0.004f)
	,	rolloffFactor	(0.5f)
	,	feedbackFactor	(0.09f)
	,	blockSizeAction	(blockSize)
	,	kernelSizeAction(kernelSize)
	,	kernelFilterAction(filterAction)
	,	timeSinceLastFilterAction(0)
	,	timeSinceLastResizeAction(0)
{
	seed = Time::currentTimeMillis();

	memory.resize(256);
	noiseArray = memory.place(256);
	noiseArray.rand(seed);
//	setPendingAction(kernelSize, 131072);
}

String numfill(int number) {
	return String(number) + (number < 1000 ? "\t\t" : "\t");
}

void ReverbEffect::processBuffer(AudioSampleBuffer &buffer) {
	StereoBuffer input(buffer);

	if (outBuffer.left.empty())
		return;

	//	if(calls == 0)
	//	{
	//		dout << "samp\tcume\tf.re\tf.wr\tp.re\tp.wr\t\n";
	//	}

	int numSamples = buffer.getNumSamples();

	ConvReverb *convolvers[] = {&leftConv, &rightConv};

	for (int i = 0; i < buffer.getNumChannels(); ++i) {
		Buffer<float> out = outBuffer[i].withSize(numSamples);
		convolvers[i]->process(input[i], out);
	}

	if (buffer.getNumChannels() == 1) {
		input.left.mul(1 - 0.25 * wetLevel).addProduct(outBuffer.left, wetLevel);
	} else {
		Buffer<float> merge = mergeBuffer.withSize(numSamples);
		float level = 1 - 0.24f * wetLevel

		merge.mul(outBuffer.left, 			wetLevel * jmax(0.5f, width));
		merge.addProduct(outBuffer.right, 	wetLevel * jmin(0.5f, 1.f - width));

		input.left.mul(level).add(merge);

		merge.mul(outBuffer.right, 			wetLevel * jmax(0.5f, width));
		merge.addProduct(outBuffer.left, 	wetLevel * jmin(0.5f, 1.f - width));

		input.right.mul(level).add(merge);
	}
}


bool ReverbEffect::doParamChange(int index, double value, bool doFutherUpdate)
{
	juce::Reverb::Parameters parameters = model.getParameters();

	switch (index) {
		case Size: {
			roomSize = value;

			int length = NumberUtils::nextPower2((int) powf(2, 12 + 6 * roomSize));
			if (length != kernel[0].size()) {
				setPendingAction(kernelSize, length);
			}

			break;
		}

		case Damp:
			rolloffFactor = value * 0.7;
			setPendingAction(filterAction, 0);

			break;

		case Width:
			width = value;
			break;

		case Highpass:
			highpass = value;
			setPendingAction(filterAction, 0);
			break;

		case Wet:
			//			parameters.wetLevel = value;
			wetLevel = 0.25f * value;
			break;
		default:
			throw std::invalid_argument("ReverbEffect::doParamChange, Invalid Parameter Index");
	}

	//	model.setParameters(parameters);

	return false;
}

bool ReverbEffect::isEnabled() const {
	return ui->isEffectEnabled();
}

void ReverbEffect::setPendingAction(int action, int value) {
	switch (action) {
		case blockSize: {
			blockSizeAction.setValueAndTrigger(value);
			break;
		}

		case kernelSize:
			kernelSizeAction.setValueAndTrigger(value);
			break;

		case filterAction: {
			if (Time::currentTimeMillis() - timeSinceLastFilterAction >= 50) {
				timeSinceLastFilterAction = Time::currentTimeMillis();
				kernelFilterAction.trigger();
			} else {
				kernelFilterAction.dismiss();

				stopTimer(kernelSize);
				startTimer(kernelSize, 30);
			}
			break;
		}
		default:
			throw std::invalid_argument("ReverbEffect::setPendingAction, Invalid Parameter Index");
	}
}

void ReverbEffect::timerCallback(int id) {
	switch (id) {
		case blockSize:
			setPendingAction(blockSize, blockSizeAction.getValue());
			break;

		case kernelSize:
			setPendingAction(filterAction, 0);
			break;
		default:
			throw std::invalid_argument("ReverbEffect::timerCallback, Invalid Parameter Index");
	}

	stopTimer(id);
}

void ReverbEffect::createKernel(int size) {
	jassert(size > 0);

	int nextPow2 = NumberUtils::nextPower2(size);

	if (nextPow2 < 256) {
		return;
	}

	kernelMemory.resize(nextPow2 * 2);

	for (int c = 0; c < 2; ++c) {
		kernel[c] = kernelMemory.section(c * nextPow2, nextPow2);
	}

	updateKernelSections();
}

void ReverbEffect::updateKernelSections()
{
	int buffSize = 256;

	Transform fft;
	fft.allocate(buffSize, true);

	ScopedAlloc<float> mem(buffSize * 10);

	Buffer<float> cumeRoll 	= mem.place(buffSize);
	Buffer<float> magn 		= mem.place(buffSize);
	Buffer<float> phs 		= mem.place(buffSize);
	Buffer<float> noise		= mem.place(buffSize);
	Buffer<float> rolloff	= mem.place(buffSize);
	Buffer<float> volRamp	= mem.place(buffSize);
	Buffer<float> filtered	= mem.place(buffSize);
	Buffer<float> filteredA	= mem.place(buffSize);
	Buffer<float> fwdRamp	= mem.place(buffSize);
	Buffer<float> invRamp	= mem.place(buffSize);

	{
		int highStart 		= jmax(2, int(highpass * 0.05f * buffSize));
		float highLevelA 	= 1 - rolloffFactor * highpass * 1.5f;
		float highLevelRamp = (1 - highLevelA) / float(highStart);

		rolloff.zero();
		rolloff.ramp(1.f, -rolloffFactor / float(buffSize));
		fwdRamp.section(0, highStart).ramp(highLevelA, highLevelRamp);
		rolloff.section(0, highStart).mul(fwdRamp);
		rolloff.mul(IPP_PI2).sin();
	}

	fwdRamp.ramp();
	fwdRamp.copyTo(invRamp);
	invRamp.subCRev(1.f);

	unsigned seed = buffSize ^ 0xc0d3db4d;

	const int numBuffers  = kernel.left.size() / buffSize;

	for(int c = 0; c < 2; ++c) {
		cumeRoll.set(1.f);

		for(int i = 0; i < numBuffers; ++i) {
			Buffer<float> section = kernel[c].section(i * buffSize, buffSize);

			createVolumeRamp(i, numBuffers, buffSize, volRamp);

			noise	.rand(seed);
			fft		.forward(noise);
			fft		.getMagnitudes().mul(cumeRoll);
			cumeRoll.mul(rolloff);

			fft		.inverse(filteredA);
			fft		.getMagnitudes().mul(rolloff);
			fft		.inverse(filtered);

			section	.mul(filteredA, invRamp);
			section	.addProduct(fwdRamp, filtered);
			section	.mul(volRamp);

			seed ^= 616137 * seed;
		}
	}

	leftConv .init(leftConv.headBlockSize, 	leftConv.tailBlockSize,  kernel.left);
	rightConv.init(rightConv.headBlockSize, rightConv.tailBlockSize, kernel.right);
}

// TODO why is half of this commented out?
void ReverbEffect::createVolumeRamp(int i, int numBuffers, int buffSize, Buffer<float> ramp) const {
	const int rampUpBuffs = jlimit(3, 20, int(roomSize * 0.03f * numBuffers));
	float rampStart, rampEnd, rampDelta;

//	if(i < rampUpBuffs)
//	{
//		float start = i / float(rampUpBuffs - 1);
//		float end 	= (i + 1) / float(rampUpBuffs - 1);
//
//		rampStart 	= start;
//		rampEnd   	= end;
//		rampDelta 	= (rampEnd - rampStart) / float(buffSize);
//	}
//	else
//	{
		float scale = IPP_PI2 * (float(NumberUtils::log2i(numBuffers) + 8) * 0.1f - 0.6);

		rampStart 	= 0.25f / scale * sinf(scale * expf(i * -4.5f / float(numBuffers))) * sqrtf(float(numBuffers - i) / numBuffers);
		rampEnd 	= 0.25f / scale * sinf(scale * expf((i + 1) * -4.5f / float(numBuffers))) * sqrtf(float(numBuffers - (i + 1)) / numBuffers);

//		rampStart 	= (rampUpBuffs + 3) / float(i + rampUpBuffs + 4) * invRamp;
//		rampEnd   	= (rampUpBuffs + 3) / float(i + rampUpBuffs + 5) * invRamp;
		rampDelta 	= (rampEnd - rampStart) / float(buffSize);
//	}

	if(i < rampUpBuffs) {
		/*


		if(silence < buffSize)
		{
			for(int j = 0; j <= 5 * (i + 2); ++j)
			{
				ramp[delay1 & mod] = noiseArray[delay2 & mod];
				ramp[delay2 & mod] = noiseArray[delay1 & mod];

				delay1 += 71;
				delay2 *= 13;
			}


			int offset = delay1 & mod;
			for(int j = 0; j <= i; ++j)
			{
				if(offset < buffSize)
					ramp.addProduct(ramp.section(offset, buffSize - offset), -0.5f);

				if(offset > 0)
					ramp.section(buffSize - offset, offset).addProduct(ramp, -0.5f);

				offset = (offset + delay1) & mod;
			}

		}

		ramp.zero();
		*/

		float start = 0.25f * float(i) / float(rampUpBuffs - 1);
		float end 	= 0.25f * float(i + 1) / float(rampUpBuffs - 1);

		rampStart 	= start;
		rampEnd   	= end;
		rampDelta 	= (end - start) / float(buffSize);
	}
	else
	{
	}

	ramp.ramp(rampStart, rampDelta);

	int silence = roomSize * 450 - i * buffSize;

	if(silence > 0) {
		ramp.zero(jmin(buffSize, silence));
	}
}

void ReverbEffect::setBlockSize(int size) {
	int nextPow2 = NumberUtils::nextPower2(size);

	blockMemory.ensureSize(size * 3);
	mergeBuffer = blockMemory.place(size);
	mergeBuffer.zero();

	ConvReverb* convolvers[] = { &leftConv, &rightConv };

	for(int i = 0; i < 2; ++i) {
		outBuffer[i] = blockMemory.place(size);
		outBuffer[i].zero();

		convolvers[i]->init(nextPow2, 16 * nextPow2, kernel[i]);
	}
}

void ReverbEffect::audioThreadUpdate() {
	if(blockSizeAction.isPending()) {
		setBlockSize(blockSizeAction.getValueAndDismiss());
	}

	if(kernelSizeAction.isPending()) {
		createKernel(kernelSizeAction.getValueAndDismiss());
	}

	if(kernelFilterAction.isPending()) {
		updateKernelSections();
		kernelFilterAction.dismiss();
	}
}

void ReverbEffect::resetOutputBuffer() {
	outBuffer.left.zero();
	outBuffer.right.zero();
}

void ReverbEffect::randomizePhase(Buffer<float> buffer) {
	/*
	fft.forward(buffer);
	fft.getPhases().rand(seed);
	fft.inverse(phaseBuffer);

	phaseBuffer.mul(hannWindow);
	*/
}
