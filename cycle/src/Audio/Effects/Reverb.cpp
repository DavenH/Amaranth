#include <Algo/ConvReverb.h>
#include <App/SingletonRepo.h>
#include <Audio/CycleDsp/ReverbKernel.h>
#include <Util/Arithmetic.h>

#include "Reverb.h"

#include "../../UI/Effects/GuilessEffect.h"
#include "../../UI/Effects/ReverbUI.h"

ReverbEffect::ReverbEffect(SingletonRepo* repo) :
        Effect			(repo, "ReverbEffect")
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

    if (outBuffer.left.empty()) {
        return;
    }

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
        float level = 1 - 0.24f * wetLevel;

        VecOps::mul(outBuffer.left, wetLevel * jmax(0.5f, width), merge);
        merge.addProduct(outBuffer.right, wetLevel * jmin(0.5f, 1.f - width));

        input.left.mul(level).add(merge);

        VecOps::mul(outBuffer.right, wetLevel * jmax(0.5f, width), merge);
        merge.addProduct(outBuffer.left, wetLevel * jmin(0.5f, 1.f - width));

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
    CycleDsp::ReverbKernelConfiguration configuration;
    configuration.roomSize = roomSize;
    configuration.damping = rolloffFactor;
    configuration.highPass = highpass;
    CycleDsp::buildReverbKernel(configuration, kernel.left, kernel.right);

    leftConv .init(leftConv.headBlockSize, 	leftConv.tailBlockSize,  kernel.left);
    rightConv.init(rightConv.headBlockSize, rightConv.tailBlockSize, kernel.right);
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
