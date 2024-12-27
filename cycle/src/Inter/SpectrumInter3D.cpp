#include <Algo/PitchTracker.h>
#include <App/MeshLibrary.h>
#include <Obj/Ref.h>
#include <UI/Panels/Panel.h>
#include "SpectrumInter3D.h"
#include "../Audio/AudioSourceRepo.h"
#include "../Audio/SynthAudioSource.h"
#include "../Inter/SpectrumInter2D.h"
#include "../UI/Widgets/MidiKeyboard.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/Panels/OscControlPanel.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"
#include <Definitions.h>

SpectrumInter3D::SpectrumInter3D(SingletonRepo* repo) :
        Interactor3D(repo, "SpectrumInter3D"), SingletonAccessor(repo, "SpectrumInter3D") {
    selectionClient = nullptr;
}

void SpectrumInter3D::init() {
    Interactor3D::init();

    jassert(selectionClient == nullptr);

    updateSource = UpdateSources::SourceSpectrum3D;
    layerType = LayerGroups::GroupSpect;
    scratchesTime = true;

    vertexProps.dimensionNames.set(Vertex::Phase, "Freq");
    vertexProps.dimensionNames.set(Vertex::Amp, "Magn");
    vertexProps.ampVsPhaseApplicable = true;

    vertexLimits[Vertex::Phase].setEnd(1.f + getRealConstant(FreqMargin));
    vertexLimits[Vertex::Phase].setStart(-getRealConstant(FreqMargin));

    vertsAreWaveApplicable = true;

    selectionClient = std::make_unique<MeshSelectionClient3D>(this, repo, &getObj(EditWatcher), &getObj(MeshLibrary));

    // by this point the rasterizer better be set!
    selectionClient->initialise(this, rasterizer, layerType);
}

void SpectrumInter3D::initSelectionClient() {
}

void SpectrumInter3D::updateSelectionClient() {
    selectionClient->initialise(this, rasterizer, layerType);
}

void SpectrumInter3D::doExtraMouseUp() {
    Interactor3D::doExtraMouseUp();

    getObj(SynthAudioSource).enablementChanged();
}

void SpectrumInter3D::meshSelectionChanged(Mesh* mesh) {
    updateInterceptsWithMesh(mesh);
    display->repaint();
    getObj(SpectrumInter2D).update(Update);
}

bool SpectrumInter3D::isCurrentMeshActive() {
    int currentGroup = getSetting(MagnitudeDrawMode) == 1 ? LayerGroups::GroupSpect : LayerGroups::GroupPhase;
    MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentProps(currentGroup);

    return props != nullptr && props->active;
}

String SpectrumInter3D::getZString(float tableValue, int xIndex) {
    String zString;

    if (getSetting(MagnitudeDrawMode)) {
        float absAmp = 2 * Arithmetic::invLogMapping((float) getConstant(FFTLogTensionAmp) * IPP_2PI, tableValue);
        zString = Util::getDecibelString(absAmp);
    } else {

        float phaseScale = powf(2, getObj(Spectrum3D).getScaleFactor()) * sqrtf(xIndex + 1);
        zString = String((tableValue - 0.5f) * phaseScale, 2) + String(L" \u03c0");
    }

    return zString;
}

int SpectrumInter3D::getTableIndexY(float y, int size) {
    float freqTens = size * getRealConstant(FreqTensionScale);
    int yIndex = jmax(0, int(Arithmetic::invLogMapping(freqTens, y, true) * (size - 1)));

    return yIndex;
}

String SpectrumInter3D::getYString(float yVal, int yIndex, const Column &col, float fundFreq) {
    if (yIndex < 0) {
        return String(yVal, 3);
    }

    int harmonicNum = yIndex + 1;

    return "#" + String(harmonicNum) + " (" + String(int(fundFreq * harmonicNum)) + "Hz)";
}

void SpectrumInter3D::updateRastDims() {
    rasterizer->setDims(getObj(SpectrumInter2D).dims);
}

Interactor* SpectrumInter3D::getOppositeInteractor() {
    return &getObj(SpectrumInter2D);
}

void SpectrumInter3D::enterClientLock(bool audioThreadApplicable) {
    if (audioThreadApplicable) {
        getObj(SynthAudioSource).getLock().enter();
    }

    panel->getRenderLock().enter();
}

void SpectrumInter3D::meshSelectionFinished() {
    getObj(SynthAudioSource).enablementChanged();
}

void SpectrumInter3D::exitClientLock(bool audioThreadApplicable) {
    panel->getRenderLock().exit();

    if (audioThreadApplicable) {
        getObj(SynthAudioSource).getLock().exit();
    }
}

