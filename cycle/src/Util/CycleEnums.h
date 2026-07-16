#pragma once

#include <App/MeshLibrary.h>
#include <App/AppConstants.h>
#include <App/Settings.h>

namespace UpdateSources {
    enum {
        SourceNull,
        SourceAll,
        SourceAllButFX,
        SourceMorph,
        SourceWaveform2D,
        SourceWaveform3D,
        SourceSpectrum2D,
        SourceSpectrum3D,
        SourceEnvelope2D,
        SourceEnvelope3D,
        SourceGuideCurve,
        SourceScratch,
        SourceUnison,
        SourceOscCtrls,
        SourceWaveshaper,
        SourceIrModeller,
        SourceEqualizer
    };
}

namespace EffectTypes {
    enum {
        TypeWaveshaper, TypeIrModeller, TypeDelay, TypeReverb, TypeUnison, TypePhaser, TypeChorus, TypeEqualizer
    };
}

namespace ViewStages {
    enum {
        PreProcessing, PostEnvelopes, PostSpectrum, PostFX
    };
}

namespace WindowSizes {
    enum {
        PlayerSize, SmallSize, MedSize, FullSize
    };
}

namespace Constants {
    enum {
        // keys
        WaveshaperPadding = numAppConstants + 1
    ,   MaxDetune
    ,   BuildNumber
    ,   MaxNumVoices
    ,   BetaExpiry
    ,   ProductCode
    ,   AmpTensionScale
    ,   FreqMargin
    ,   EnvResolution
    ,   MaxBufferSize
    ,   SpectralMargin
    ,   MaxCyclePeriod
    ,   MaxUnisonOrder
    ,   GuideCurvePadding
    ,   ResamplerLatency
    ,   IrModellerPadding
    ,   ControllerValueSaturation
    ,   numCycleConstants
    };
}

namespace AppSettings {
    enum {
        WindowSize = numSettings + 1,
        ViewStage,
        IgnoringMessages,
        MagnitudeDrawMode,
        PitchAlgo,
        CurrentEnvGroup,
        DrawWave,
        InterpWaveCycles,
        WrapWaveCycles,
        Waterfall,
        WaveLoaded,
        DebugLogging,
        ReductionFactor,
        NativeDialogs,
        FilterEnabled,
        PhaseEnabled,
        PlayerSize,
        TimeEnabled,
        UseYellowDepth,
        UseRedDepth,
        UseBlueDepth,
        UseLargerPoints
    };
}

namespace DocSettings {
    enum {
        Declick,
        ControlFreq,
        SubsampleRend,
        SubsampleRltm,
        PitchBendRange,
        DynamicEnvelopes,
        ParameterSmoothing,
        ResamplingAlgoRltm,
        ResamplingAlgoRend,
        OversampleFactorRltm,
        OversampleFactorRend
    };
}


namespace DialogActions {
    enum {
        TrackPitchAction, LoadMultisample, LoadImpulse
    };
}

namespace PitchAlgos {
    enum {
        AlgoAuto, AlgoYin, AlgoSwipe
    };
}
