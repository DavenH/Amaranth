#include <Algo/AutoModeller.h>
#include <App/SingletonRepo.h>
#include <App/Settings.h>

#include "WaveformInter2D.h"
#include "Algo/Resampling.h"
#include "../Audio/SynthAudioSource.h"
#include "../Inter/WaveformInter3D.h"
#include "../UI/VisualDsp.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../Util/CycleEnums.h"

WaveformInter2D::WaveformInter2D(SingletonRepo* repo) :
        Interactor2D(repo, "WaveformInter2D",
                     Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue)),
        SingletonAccessor(repo, "WaveformInter2D") {
    updateSource = UpdateSources::SourceWaveform2D;
    layerType = LayerGroups::GroupTime;
    scratchesTime = true;

    vertsAreWaveApplicable = true;
    vertexLimits[Vertex::Phase].setEnd(1.9999f);

    vertexProps.ampVsPhaseApplicable = true;
}

void WaveformInter2D::showCoordinates() {
    int lockId = -1;
    const vector <Column> &timeColumns = getObj(VisualDsp).getTimeColumns();

    if (timeColumns.size() < 2) {
        Interactor::showCoordinates();
        return;
    }

    int size = (int) timeColumns.size() - 1;
    float index = jlimit(0, size - 1, int(getObj(PlaybackPanel).getX() * size));

    const Column &col = timeColumns[index];
    float columnVal = Resampling::lerpC(col, state.currentMouse.x);
    float bipolarVal = 2 * columnVal;
    String coords = String(state.currentMouse.x * 2, 2) + String(L" \u03c0") + ", " + String(bipolarVal, 2);

    showMsg(coords);
}

bool WaveformInter2D::isCurrentMeshActive() {
    return getObj(WaveformInter3D).isCurrentMeshActive();
}

Interactor* WaveformInter2D::getOppositeInteractor() {
    return &getObj(WaveformInter3D);
}

void WaveformInter2D::modelAudioCycle() {
    if (!getSetting(WaveLoaded)) {
        showMsg("Load a wave file first!");
        return;
    }

    ScopedAlloc <Ipp32f> columnCopy;

    {
        const vector <Column> &timeColumns = getObj(VisualDsp).getTimeColumns();
        float progress = getObj(PlaybackPanel).getProgress();
        const Column &column = timeColumns[progress * ((int) timeColumns.size() - 1)];
        CriticalSection &lock = getObj(Spectrum3D).getRenderer()->getGridLock();

        ScopedLock sl(lock);

        columnCopy.ensureSize(column.size());
        column.copyTo(columnCopy);
    }

    {
        ScopedLock sl(getObj(SynthAudioSource).getLock());

        AutoModeller modeller;
        modeller.modelToInteractor(columnCopy, this, true, 0, 0.3f);
    }

    postUpdateMessage();
}
