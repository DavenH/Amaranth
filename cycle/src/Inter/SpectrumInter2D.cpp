#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/Multisample.h>
#include <Audio/PitchedSample.h>
#include <cmath>
#include <Util/Arithmetic.h>
#include <Util/LogRegions.h>

#include "SpectrumInter2D.h"
#include "../Audio/SynthAudioSource.h"
#include "../Inter/SpectrumInter3D.h"
#include "../UI/Panels/Console.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"


SpectrumInter2D::SpectrumInter2D(SingletonRepo* repo) :
        Interactor2D(repo, "SpectrumInter2D",
                     Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue)),
        SingletonAccessor(repo, "SpectrumInter2D") {
}

void SpectrumInter2D::init() {
    spectrum3D = &getObj(Spectrum3D);
    updateSource = UpdateSources::SourceSpectrum2D;
    layerType = LayerGroups::GroupSpect;
    closestHarmonic = -1;
    scratchesTime = true;

    float freqMargin = getRealConstant(FreqMargin);

    vertexProps.dimensionNames.set(Vertex::Phase, "Freq");
    vertexProps.dimensionNames.set(Vertex::Amp, "Magn");
    vertexProps.ampVsPhaseApplicable = true;

    vertexLimits[Vertex::Phase].setEnd(1.f + freqMargin);
    vertexLimits[Vertex::Phase].setStart(-freqMargin);
}

bool SpectrumInter2D::locateClosestElement() {
    bool changedElement = Interactor2D::locateClosestElement();

    int oldHarmonic = closestHarmonic;
    ScopedLock sl(vertexLock);

    const vector <Column> &columns = getObj(VisualDsp).getFreqColumns();
    if (columns.empty()) {
        closestHarmonic = 0;
        return changedElement;
    }

    int index = (columns.size() - 1) * getObj(PlaybackPanel).getProgress();

    Buffer <Float32> ramp = getObj(LogRegions).getRegion(columns[index].midiKey);

    float freqTens = ramp.size() * getRealConstant(FreqTensionScale);
    float invScale = Arithmetic::invLogMapping(freqTens, state.currentMouse.x, true);
    closestHarmonic = jlimit(0, ramp.size() - 1, int(invScale * ramp.size()));

    jassert(closestHarmonic >= 0 && closestHarmonic < ramp.size());

    if (oldHarmonic != closestHarmonic) {
        flag(SimpleRepaint) = true;
    }

    return changedElement;
}

void SpectrumInter2D::doExtraMouseUp() {
    if (getMesh()->getNumCubes() < 6) {
        getObj(SynthAudioSource).enablementChanged();
    }
}

void SpectrumInter2D::showCoordinates() {
    const vector <Column> &columns = getObj(VisualDsp).getFreqColumns();

    if (columns.empty()) {
        Interactor::showCoordinates();
        return;
    }

    const int currHarmonic = closestHarmonic;

    if (currHarmonic >= columns.front().size() || currHarmonic < 0) {
        Interactor::showCoordinates();
        return;
    }

    int index = (columns.size() - 1) * getObj(PlaybackPanel).getProgress();
    const Column& col = columns[index];

    bool isMagnitudeMode = getSetting(MagnitudeDrawMode) == 1;

    String yString;
    if (isMagnitudeMode) {
        float absAmp =
                2 * Arithmetic::invLogMapping((float) getConstant(FFTLogTensionAmp) * IPP_2PI, col[currHarmonic]);
        yString = Util::getDecibelString(absAmp);
    } else {
        float phaseScale = powf(2, spectrum3D->getScaleFactor()) * sqrtf(currHarmonic + 1);
        yString = String((col[currHarmonic] - 0.5f) * phaseScale, 2) + String(L"\u03c0");
    }

    int displayKey = col.midiKey - 12;
    float fundFreq = MidiMessage::getMidiNoteInHertz(displayKey);

    String message =
            "#" + String(currHarmonic + 1) + " (" + String(int(fundFreq * (currHarmonic + 1))) + "Hz), " + yString;

    showConsoleMsg(message);
}

bool SpectrumInter2D::isCurrentMeshActive() {
    return getObj(SpectrumInter3D).isCurrentMeshActive();
}

Interactor* SpectrumInter2D::getOppositeInteractor() {
    return &getObj(SpectrumInter3D);
}
