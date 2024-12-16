#include <Array/Buffer.h>
#include <Util/StatusChecker.h>
#include <Util/Util.h>
#include "IrModeller.h"

#include <Design/Updating/Updater.h>

#include "../../Audio/SynthAudioSource.h"
#include "../../UI/Effects/IrModellerUI.h"
#include "../../UI/VisualDsp.h"
#include "../../UI/VertexPanels/EffectPanel.h"
#include "../../UI/Panels/MainPanel.h"
#include "../../Util/CycleEnums.h"


IrModeller::IrModeller(SingletonRepo *repo) :
        Effect(repo, "IrModeller")
    ,   postamp(1.f)
    ,   accumWritePos(512)
    ,   convReadPos(512)
    ,   convBufferSize(512)
    ,   output(2)
    ,   enabled(false)
    ,   waveLoaded(false)
    ,   usingWavFile(false)
    ,   unloadWavAction(unloadWav)
    ,   convSizeAction(convSize)
    ,   impulseSizeAction(impulseSize)
    ,   blockSizeAction(blockSize)
    ,   prefiltAction(prefilterChg)
    ,   rasterizeAction(rasterize)
    ,   audioThdRasterizer("ImpulseRasterizerAudioThd")
    ,   oversampler(repo, 8)
    ,   wavImpulse(repo) {
    setConvBufferSize(convBufferSize);
    audioThdRasterizer.setScalingMode(MeshRasterizer::Bipolar);

    prefilt.setSmoothingActivity(false);
    oversampler.setOversampleFactor(2);

    pendingActions.add(&impulseSizeAction);
    pendingActions.add(&convSizeAction);
    pendingActions.add(&blockSizeAction);
    pendingActions.add(&unloadWavAction);
    pendingActions.add(&prefiltAction);
    pendingActions.add(&rasterizeAction);

    audio.convolvers.push_back(&convolvers[0]);
    audio.convolvers.push_back(&convolvers[1]);
    graphic.convolvers.push_back(&graphicConv);
}


IrModeller::~IrModeller() {
    cleanUp();

    wavImpulse.clear();
}

void IrModeller::doPostWaveLoad() {
    trimWave();

    int greaterPow2 = Arithmetic::getNextPow2((float) wavImpulse.buffer.getNumSamples());
    ui->getParamGroup().setKnobValue(Length, calcKnobValue(greaterPow2), false);

    usingWavFile = true;
    waveLoaded = true;

    rasterizeAction.trigger();
}

void IrModeller::doPostDeconvolve(int size) {
    usingWavFile = false;

    rasterizeAction.trigger();
}

void IrModeller::trimWave() {
    int size = wavImpulse.buffer.getNumSamples();

    if (size < 64)
        return;

    static const float trimThres = 0.001f;

    float movingAverage = 0;

    int threshIdx = size - 1;

    Buffer<float> channel(wavImpulse.buffer, 0);

    for (int i = 0; i < size; ++i) {
        threshIdx = size - i - 1;
        movingAverage = 0.95f * movingAverage + 0.05f * fabsf(channel[threshIdx]);

        if (movingAverage > trimThres)
            break;
    }

    if (threshIdx == 0)
        showImportant("Wave file is silent");

    int newSize = threshIdx + 1;
    NumberUtils::constrain(newSize, 64, 16384);
    int diff = size - newSize;

    if (diff > 0)
        wavImpulse.buffer.setSize(wavImpulse.buffer.getNumChannels(), newSize, true, true);
}

void IrModeller::rasterizeImpulseDirect() {
    rasterizeImpulse(audio.rawImpulse, audioThdRasterizer, true);
    filterImpulse(audio);

    for (int i = 0; i < 2; ++i) {
        ::BlockConvolver &conv = convolvers[i];
        conv.init(conv.getBlockSize(), audio.impulse);
    }
}

void IrModeller::rasterizeGraphicImpulse() {
    Buffer<float> impulse = graphic.rawImpulse;

    if (impulse.empty())
        return;

    if (usingWavFile) {
        int wavSize = wavImpulse.buffer.getNumSamples();
        int maxSamples = jmin(wavSize, impulse.size());

        Buffer<float> channel(wavImpulse.buffer, 0);
        channel.copyTo(impulse);

        if (maxSamples < impulse.size())
            impulse.offset(maxSamples).zero();
    } else {
        FXRasterizer &graphicRast = *dynamic_cast<FXRasterizer *>(ui->getRasterizer());
        rasterizeImpulse(impulse, graphicRast, false);
    }

    filterImpulse(graphic);
    graphicConv.init(graphicConv.getBlockSize(), graphic.impulse);
}

void IrModeller::filterImpulse(ConvState &chan) {
    int size = chan.impulse.size();
    int halfSize = size / 2;
    bool isGraphic = (&chan == &graphic);

    Buffer<float> levelBuff = isGraphic ? graphic.levels : audio.levels;
    Buffer<float> mags = chan.fft.getMagnitudes();

    chan.fft.forward(chan.rawImpulse);
    mags.mul(levelBuff);
    chan.fft.inverse(chan.impulse);

    if (isGraphic) {
        Arithmetic::applyLogMapping(mags, 1000);
        mags.threshLT(0.f).mul(0.99f / mags.max());
    }
}

void IrModeller::rasterizeImpulse(Buffer<float> impulse, FXRasterizer &rast, bool isAudioThread) {
    if (impulse.empty()) {
        return;
    }

    Ipp32f sum = 0;

    if (!usingWavFile) {
        if (!rast.hasEnoughCubesForCrossSection()) {
            return;
        }

        rast.performUpdate(UpdateType::Update);

        double delta = (1.f - getRealConstant(IrModellerPadding)) / double(impulse.size() - 1);
        double phase = getRealConstant(IrModellerPadding);

        if (isAudioThread) {
            delta /= (double) oversampler.getOversampleFactor();
            int samplingSize = impulse.size() * oversampler.getOversampleFactor();

            Buffer <Ipp32f> buff = oversampler.getMemoryBuffer(samplingSize);
            rast.samplePerfectly(delta, buff, phase);

            oversampler.sampleDown(buff, impulse);
        } else {
            rast.sampleWithInterval(impulse, delta, phase);
        }

        if (!rast.isBipolar()) {
            impulse.mul(2.f).add(-1.f);
        }

//		sum = impulse.sum();
    } else {
        Buffer<float> wavBuff(wavImpulse.buffer, 0);
        int maxSamples = jmin(wavBuff.size(), impulse.size());

        wavBuff.copyTo(impulse);

        if (maxSamples < impulse.size()) {
            impulse.offset(maxSamples).zero();
        }
    }
}

void IrModeller::audioThreadUpdate() {
    checkForPendingUpdates();
}

void IrModeller::processBuffer(AudioSampleBuffer &buffer) {
    if (audio.impulse.empty()) {
        return;
    }

    int numSamples = buffer.getNumSamples();
    jassert(numSamples <= audio.blockSize);

    if (numSamples == 0) {
        return;
    }

    StereoBuffer input(buffer);

    for (int i = 0; i < buffer.getNumChannels(); ++i) {
        output[i].zero();
        convolvers[i].process(input[i], output[i]);
        output[i].mul(postamp).copyTo(input[i]);
    }
}

void IrModeller::initGraphicVars() {
}

void IrModeller::checkForPendingUpdates() {
    bool oldEnabled = enabled;
    enabled = ui->isEffectEnabled();

    if (oldEnabled != enabled)
        resetIndices();

    for (int i = 0; i < pendingActions.size(); ++i) {
        PendingAction *action = pendingActions.getReference(i);

        if (action->isPending()) {
            switch (action->getId()) {
                case impulseSize:
                    setAudioImpulseLength(impulseSizeAction.getValue());
                    break;
                case convSize:
                    setConvBufferSize(convSizeAction.getValue());
                    break;
                case blockSize:
                    setAudioBlockSize(blockSizeAction.getValue());
                    break;

                case rasterize: {
                    audioThdRasterizer.calcCrossPoints(0);
                    rasterizeImpulseDirect();
                    break;
                }

                case unloadWav:
                    unloadWave();
                    break;

                case prefilterChg:
                    calcPrefiltLevels(audio.levels);
                    filterImpulse(audio);
                    convolvers[0].init(convolvers[0].getBlockSize(), audio.impulse);
                    convolvers[1].init(convolvers[1].getBlockSize(), audio.impulse);
                    break;
                default: throw std::invalid_argument("Illegal IrModeller state: " + std::to_string(action->getId()));
            }
        }

        action->dismiss();
    }
}

void IrModeller::processVertexBuffer(Buffer <Ipp32f> inputBuffer) {
    if (!graphic.impulse.empty())
        graphicConv.process(inputBuffer, inputBuffer);

    inputBuffer.mul((float) postamp.getTargetValue());
}

void IrModeller::cleanUp() {
}

void IrModeller::setImpulseLength(ConvState &state, int length) {
    if (length == 0)
        return;

    bool sizeChanged = length != state.impulse.size();

    if (sizeChanged) {
        state.memory.resize(2 * length + length / 2);
        state.memory.zero();

        state.rawImpulse = state.memory.place(length);
        state.impulse = state.memory.place(length);
        state.levels = state.memory.place(length / 2);

        state.fft.allocate(length, true);

        calcPrefiltLevels(state.levels);
    }

    if (&state == &audio) {
        rasterizeImpulseDirect();
    } else {
        rasterizeGraphicImpulse();
    }
}

void IrModeller::setAudioImpulseLength(int length) {
    setImpulseLength(audio, length);
}

void IrModeller::setGraphicImpulseLength(int length) {
    setImpulseLength(graphic, length);
}

void IrModeller::clearGraphicOverlaps() {
}

void IrModeller::setPendingAction(PendingUpdate type, int value) {
    switch (type) {
        case impulseSize:
            setGraphicImpulseLength(value);
            impulseSizeAction.setValueAndTrigger(value);
            break;

        case blockSize:
            blockSizeAction.setValueAndTrigger(value);
            break;

        case convSize:
            convSizeAction.setValueAndTrigger(value);
            break;

        case unloadWav:
            usingWavFile = false;
            setGraphicImpulseLength(calcLength(ui->getParamGroup().getKnobValue(Length)));
            rasterizeGraphicImpulse();
            unloadWavAction.trigger();
            break;

        case rasterize:
            rasterizeGraphicImpulse();
            rasterizeAction.trigger();
            break;

        case prefilterChg:
            calcPrefiltLevels(graphic.levels);
            prefiltAction.trigger();
            break;
    }
}

void IrModeller::setMesh(Mesh *mesh) {
    audioThdRasterizer.setMesh(mesh);
}

void IrModeller::setUI(IrModellerUI *comp) {
    ui = comp;

//	setMesh(ui->getRasterizer()->getMesh());
//	setPendingRasterize();
}

void IrModeller::setConvBufferSize(int newConvBufSize) {
//	if(! Util::assignAndWereDifferent(convBufferSize, newConvBufSize))
//	{
//		return;
//	}

//	for(int i = 0; i < numElementsInArray(convStates); ++i)
//		updateConvState(convStates[i], convBufferSize);

//	if(audioBlockSize > 0)
//	{
//		for(int i = 0; i < 2; ++i)
//		{
//			accumBufs[i].resize(2 * convBufferSize + audioBlockSize);
//			accumBufs[i].zero();
//		}
//
//		resetIndices();
//	}

//	onlyPlug(getObj(PluginProcessor).updateLatency());
}

void IrModeller::setAudioBlockSize(int size) {
    if (Util::assignAndWereDifferent(audio.blockSize, size)) {
        outputMem.ensureSize(size * 2);

        output.left = outputMem.place(size);
        output.right = outputMem.place(size);

        for (int i = 0; i < 2; ++i)
            convolvers[i].init(size, audio.impulse);

        /*
        if(convBufferSize > 0)
        {
            for(int i = 0; i < 2; ++i)
            {
                accumBufs[i].resize(2 * convBufferSize + audioBlockSize);
                accumBufs[i].zero();
            }

            resetIndices();
        }
        */
    }
}

void IrModeller::resetIndices() {
    /*
    accumWritePos = convBufferSize;
    convReadPos = convBufferSize;
    */
}

void IrModeller::unloadWave() {
    wavImpulse.clear();

    usingWavFile = false;
    waveLoaded = false;

    setAudioImpulseLength(calcLength(ui->getParamGroup().getKnobValue(Length)));
    rasterizeImpulse(audio.rawImpulse, audioThdRasterizer, true);
    filterImpulse(audio);
}

void IrModeller::updateGraphicConvState(int graphicRes, bool force) {
    if (graphicRes < 0)
        return;

    if (Util::assignAndWereDifferent(graphic.blockSize, graphicRes) || force) {
        graphicConv.init(graphicRes, graphic.impulse);

        //	updateConvState(graphicConvState, graphicRes);
    }
}

bool IrModeller::willBeEnabled() const {
    return ui->isEffectEnabled();
}

bool IrModeller::doParamChange(int param, double value, bool doFurtherUpdate) {
    switch (param) {
        case Length: {
            int oldLength = audio.impulse.size();
            int length = calcLength(value);

            if (oldLength == length)
                return false;

            setPendingAction(impulseSize, length);

            return true;
        }

        case Postamp:
            return postamp.setTargetValue(calcPostamp(value));

        case Highpass: {
            bool changed = prefilt.setTargetValue(value);

            if (changed)
                setPendingAction(prefilterChg);
        }
        default:
            throw std::invalid_argument("Illegal param: " + std::to_string(param));
    }

    return true;
}

void IrModeller::updateSmoothedParameters(int deltaSamples) {
    postamp.update(deltaSamples);
}

void IrModeller::audioFileModelled() {
    usingWavFile = false;
    waveLoaded = true;
    rasterizeAction.trigger();
}

void IrModeller::calcPrefiltLevels(Buffer<float> buff) {
    double c = calcPrefilt(prefilt);

    buff.set(1.f).zero(c * buff.size());
}
