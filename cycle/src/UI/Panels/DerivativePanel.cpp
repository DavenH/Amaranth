#include <climits>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Binary/Gradients.h>
#include <Curve/RasterizerData.h>
#include <UI/IConsole.h>
#include <Util/Arithmetic.h>

#include "DerivativePanel.h"

#include <Array/VecOps.h>

#include "../VertexPanels/Waveform2D.h"
#include "../../Audio/AudioSourceRepo.h"
#include "../../Curve/GraphicRasterizer.h"
#include "../../UI/Panels/PlaybackPanel.h"
#include "../../UI/VisualDsp.h"
#include "../../Util/CycleEnums.h"


DerivativePanel::DerivativePanel(SingletonRepo* repo) :
        SingletonAccessor(repo, "DerivativePanel")
    ,   firKernel(16) {
    Image magenta = PNGImageFormat::loadFrom(Gradients::magenta_png, Gradients::magenta_pngSize);
    jassert(! magenta.isNull());

    gradient.read(magenta, false, false);
    ScopedAlloc<Float32> windowMem(firKernel.size());
    VecOps::sinc(firKernel, windowMem, 0.3f);
}

void DerivativePanel::paint(Graphics& g) {
    vector <Color>& grd = gradient.getColours();

    if (exes.empty()) {
        g.fillAll(Colours::black);
        return;
    }

    Waveform2D* wave2D = &getObj(Waveform2D);

    ColourGradient gradient;
    float rightX 		= wave2D->sx(1);
    gradient.isRadial 	= false;
    gradient.point1 	= juce::Point<float>(0, 0);
    gradient.point2 	= juce::Point<float>(rightX, 0);

    bool firstPoint = true;

    for (int i = 0; i < (int) exes.size(); ++i) {
        float x = wave2D->sx(exes[i]) / float(rightX); //(derivativeX[i] - r.x) / (r.w);
        if (x < 0 || x > 1) {
            continue;
        }

        if (firstPoint) {
            x = 0;
            firstPoint = false;
        }

        const Color& c = grd[jmin(511, indices[i] + 10)];

        gradient.addColour(double(x), c.toColour());
    }

    int right = int(rightX + 0.5f);

    g.setGradientFill(gradient);
    g.fillRect(Rectangle(0, 0, right, juce::Component::getHeight()));
    g.setColour(Colour::greyLevel(0.06f));
    g.fillRect(Rectangle(right, 0, juce::Component::getWidth() - right, juce::Component::getHeight()));
}

void DerivativePanel::mouseEnter(const MouseEvent& e) {
    showConsoleMsg("Brightest peaks show the sharpest points in the waveshape.");
}

void DerivativePanel::calcDerivative() {
    auto& timeRast = getObj(TimeRasterizer);
    RasterizerData& data = timeRast.getRastData();

    Buffer<float> waveX = data.waveX;
    Buffer<float> waveY = data.waveY;
    Buffer<float> num, dx, dy, ddy;

    int viewStage = getSetting(ViewStage);
    int size = 0;

    if (viewStage > ViewStages::PreProcessing || getSetting(DrawWave)) {
        const vector <Column>& columns = getObj(VisualDsp).getTimeColumns();

        if (columns.empty()) {
            workMemory.clear();
            exes.nullify();
            return;
        }

        float progress 	= getObj(PlaybackPanel).getProgress();
        float findex	= progress * ((int) columns.size() - 1);
        int index 		= (int) floorf(findex);
        float fraction	= findex - (float) index;

        const Column& column = columns[index];
        size = column.size();

        workMemory.ensureSize(size * 6 + firKernel.size());

        Buffer<float> sum, upsamp, filtered;
        exes= workMemory.place(size);
        dy 	= workMemory.place(size);
        ddy = workMemory.place(size);
        num = workMemory.place(size);
        sum = workMemory.place(size);
        filtered = workMemory.place(size + firKernel.size());

        exes.ramp();

        if (index < columns.size() - 1 && fraction > 0.001f) {
            VecOps::mul(column, 1 - fraction, sum);
            sum.addProduct(columns[index + 1], fraction);
        } else {
            column.copyTo(sum);
        }

        VecOps::conv(sum, firKernel, filtered);

        float iln = 1 / logf(600 + 1.f);

        VecOps::diff(filtered.section(firKernel.size() / 2, size), dy);
        VecOps::diff(dy, ddy);
        ddy.mul(600).abs().add(1.0001f).ln().mul(iln);
        ddy.copyTo(num);
    } else {
        if (waveX.empty()) {
            workMemory.clear();
            exes.nullify();
            return;
        }

        int start = data.zeroIndex;
        int end = data.oneIndex;
        size = end - start;

        if (size < 3 || end == INT_MAX) {
            workMemory.clear();
            return;
        }

        Buffer<float> dx;
        workMemory.ensureSize(size * 5);
        exes= workMemory.place(size);
        dy 	= workMemory.place(size);
        dx 	= workMemory.place(size);
        ddy = workMemory.place(size);
        num = workMemory.place(size);

        waveX.offset(start).copyTo(exes);
        VecOps::diff(exes, dx);
        VecOps::diff(waveY.section(start, size), dy);
        VecOps::diff(dy, ddy);

        VecOps::mul(ddy, dx, num);
        ddy.sqr().add(dx.sqr()).pow(1.5f);

        num.div(ddy).abs();
        num.mul(1.3f / float(size));
    }

    jassert(exes.front() < 10 && exes.front() > -10);

    num.clip(0, 1).mul(511.f);
    float m = num.max();
    indices.resize(size);

    if (m == 0) {
        indices.zero();
        return;
    }

    // scales the [0 1] ranged derivativeY into a [0 511] range index array for the colour gradient
    VecOps::roundDown(num, indices);
}

void DerivativePanel::performUpdate(UpdateType updateType) {
    calcDerivative();
    juce::Component::repaint();
}
