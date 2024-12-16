#include <UI/MiscGraphics.h>
#include <UI/Panels/Panel.h>
#include <Util/NumberUtils.h>

#include "MorphSlider.h"

#include "../Panels/Morphing/MorphPanel.h"
#include "../Panels/PlaybackPanel.h"
#include "../../Inter/WaveformInter3D.h"
#include "../VertexPanels/Waveform2D.h"
#include "../VertexPanels/Spectrum2D.h"

#include <Definitions.h>

MorphSlider::MorphSlider(
		SingletonRepo* repo
	,	const String& name
	,	const String& message
	,	bool horizontal) :
			HSlider(repo, name, message, horizontal) {
	eye = getObj(MiscGraphics).getIcon(3, 7);
	usesRightClick = true;
}

void MorphSlider::paintSpecial(Graphics& g) {
    int width = getWidth();
	int height = getHeight();

	int maxSize = hztl ? width : height;
	int start = getSliderPosition() + 2;

	NumberUtils::constrain<int>(start, 0, maxSize);
	Rectangle<int> r;

	float viewDepth = getObj(MorphPanel).getDepth(dim);

	int pos = hztl ?
			width * (getValue() + viewDepth) :
			height * (1 - getValue() - viewDepth);

	NumberUtils::constrain<int>(pos, 0, maxSize);

	g.setColour(Colour::greyLevel(0.4f).withAlpha(0.4f));

	r = hztl ? 	Rectangle<int> (start, 0, pos - start, height) :
				Rectangle<int> (0, pos, width, start - pos);

	g.fillRect(r);

	int eyePos = pos + (start - pos) / 2;

	{
		Graphics::ScopedSaveState sss(g);

		g.reduceClipRegion(r);

		hztl ?	g.drawImageAt(eye, eyePos - eye.getWidth() / 2, (height - eye.getHeight()) / 2) :
				g.drawImageAt(eye, (width - eye.getWidth()) / 2, eyePos - eye.getHeight() / 2);
	}
}

void MorphSlider::paint(Graphics& g) {
    paintFirst(g);
    paintSpecial(g);
    paintSecond(g);
}

void MorphSlider::mouseDown(const MouseEvent& e) {
    if (e.mods.isRightButtonDown())
		setDepth(e);
	else
		HSlider::mouseDown(e);
}

void MorphSlider::mouseDrag(const MouseEvent& e) {
    if (e.mods.isRightButtonDown())
        setDepth(e);
    else {
        if (!e.mods.isRightButtonDown())
            HSlider::mouseDrag(e);
//		getObj(MorphPanel).showMessage(e.x / float(getWidth()), this);
    }
}

void MorphSlider::mouseEnter(const MouseEvent& e) {
    HSlider::mouseEnter(e);

    String keys;
    switch (dim) {
        case Vertex::Time:
			keys = "PgUp, PgDn, Home, End";
			break;

		case Vertex::Red:
			keys = "(Shft +) PgUp, PgDn, Home, End";
			break;

		case Vertex::Blue:
			keys = "(Cmd +) PgUp, PgDn, Home, End";
			break;

    	default: break;
	}

	getObj(IConsole).setKeys(keys);
}

void MorphSlider::setDepth(const MouseEvent& e) {
    float x = e.x / (float) getWidth();
	float diff = jmax(0.f, x) - getValue();

	getObj(MorphPanel).setDepth(dim, fabsf(diff));

	repaint();

	if(dim == getSetting(CurrentMorphAxis))
		getObj(PlaybackPanel).repaint();

	getObj(Waveform2D).repaint();
	getObj(Spectrum2D).repaint();
}

