#include <iterator>
#include <App/MeshLibrary.h>
#include <Design/Updating/Updater.h>
#include "WaveformInter3D.h"
#include "../Audio/SynthAudioSource.h"
#include "../Inter/WaveformInter2D.h"
#include "../UI/VisualDsp.h"
#include "../UI/Panels/OscControlPanel.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/Widgets/MidiKeyboard.h"
#include "../Util/CycleEnums.h"

WaveformInter3D::WaveformInter3D(SingletonRepo* repo) :
        Interactor3D(repo, "WaveformInter3D")
    ,	SingletonAccessor(repo, "WaveformInter3D")
    ,	shiftedPhase(0) {
    canDoMiddleScroll = true;
    scratchesTime = true;
    selectionClient = 0;

    updateSource = UpdateSources::SourceWaveform3D;
    layerType = LayerGroups::GroupTime;

    vertexLimits[Vertex::Phase].setEnd(1.9999f);

    vertexProps.ampVsPhaseApplicable = true;
    vertsAreWaveApplicable = true;
}

void WaveformInter3D::doPhaseShift(float shift) {
    // this enforces the vertex y-value rules:
    // - cannot be negative
    // - cannot be greater than or equal to 2
    if (shiftedPhase + shift >= 1.f) {
        shiftedPhase = 0.f;
        shift = -0.75f;
    } else if (shiftedPhase + shift < 0) {
        shiftedPhase = 0.75f;
        shift = 0.75f;
    } else
        shiftedPhase += shift;

    for(auto vert : getMesh()->getVerts()) {
        vert->values[Vertex::Phase] += shift;
    }

    validateLinePhases();
    triggerRefreshUpdate();
}

void WaveformInter3D::doCommitPencilEditPath() {
    // todo put this in the phase envelope

    //	if(getSetting(DrawWave)) {
    //		WavWrapper& wrapper = getObj(AudioSourceRepo).getWrapper();
    //		const vector<Column>& timeColumns = getObj(VisualDsp).getTimeColumns();
    //
    //		vector<PitchFrame>& periods = wrapper.periods;
    //
    //		if(pencilPath.size() < 2)
    //			return;
    //
    //		sort(pencilPath.begin(), pencilPath.end());
    //		float lower, upper, position;
    //		lower = pencilPath.front().x;
    //		upper = pencilPath.back().x;
    //
    //		float sustainedPhaseLeft = pencilPath.front().y;
    //		float sustainedPhaseRight = pencilPath.back().y;
    //
    //		if(periods.size() != phaseOffsets.size()) {
    //			cout << "phase offsets vector not same size as period vector" << "\n";
    //			return;
    //		}
    //
    //		float invWavLength = 1 / float(wrapper.numSamples);
    //
    //		int currentIndex = 0;
    //		for(size_t i = 0; i < timeColumns.size(); ++i) {
    //			position = periods[i].sampleOffset * invWavLength;
    //
    //			if(NumberUtils::within(position, lower, upper))
    //				phaseOffsets[i] = -Arithmetic::at(position, pencilPath, currentIndex) * periods[i].period;
    //			else
    //				phaseOffsets[i] = -(fabs(position - lower) < fabs(position - upper) ? sustainedPhaseLeft :
    //																					   sustainedPhaseRight) * periods[i].period;
    //		}
    //
    //		flag(DidMeshChange) = true;
    //	}
}

void WaveformInter3D::init() {
    selectionClient = new MeshSelectionClient3D(this, repo, &getObj(EditWatcher), &getObj(MeshLibrary));

    // by this point the rasterizer better be set!
    selectionClient->initialise(this, rasterizer, layerType);
}

void WaveformInter3D::initSelectionClient() {
}

void WaveformInter3D::meshSelectionChanged(Mesh* mesh) {
    updateInterceptsWithMesh(mesh);

    getObj(WaveformInter2D).update((int) UpdateType::Update);
    display->repaint();
}

bool WaveformInter3D::isCurrentMeshActive() {
    MeshLibrary::Layer& layer = getObj(MeshLibrary).getCurrentLayer(LayerGroups::GroupTime);
    return layer.props != nullptr && layer.props->active;
}

void WaveformInter3D::doExtraMouseUp() {
    Interactor3D::doExtraMouseUp();

    getObj(SynthAudioSource).enablementChanged();
}

void WaveformInter3D::updateRastDims() {
    rasterizer->setDims(getObj(WaveformInter2D).dims);
}

Interactor* WaveformInter3D::getOppositeInteractor() {
    return &getObj(WaveformInter2D);
}

void WaveformInter3D::enterClientLock(bool audioThreadApplicable) {
    if (audioThreadApplicable)
        getObj(SynthAudioSource).getLock().enter();

    panel->getRenderLock().enter();
}

void WaveformInter3D::meshSelectionFinished() {
    getObj(SynthAudioSource).enablementChanged();
}

void WaveformInter3D::exitClientLock(bool audioThreadApplicable) {
    panel->getRenderLock().exit();

    if (audioThreadApplicable)
        getObj(SynthAudioSource).getLock().exit();
}

String WaveformInter3D::getYString(float yVal, int yIndex, const Column& col, float fundFreq) {
    return String(2 * yVal, 3) + String(L" \u03c0");
    //	return "#" + String(harmonicNum) + " (" + String(int(fundFreq * harmonicNum)) + "Hz)";
}
