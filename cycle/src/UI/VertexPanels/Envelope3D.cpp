#include <Binary/Gradients.h>

#include "Envelope3D.h"
#include "Envelope2D.h"
#include "../../Inter/EnvelopeInter3D.h"
#include "../../Curve/E3Rasterizer.h"

Envelope3D::Envelope3D(SingletonRepo* repo) :
		Panel3D(repo, "Envelope3D", this, false, true)
	,	SingletonAccessor(repo, "Envelope3D")
{
	Image blue = PNGImageFormat::loadFrom(Gradients::blue_png, Gradients::blue_pngSize);
	gradient.read(blue);
	createNameImage("Envelopes 3D");
}

void Envelope3D::buttonClicked(Button* button) {
}

Buffer<float> Envelope3D::getColumnArray() {
    return Buffer<float>(getObj(E3Rasterizer).getArray());
}

const vector<Column>& Envelope3D::getColumns() {
    return getObj(E3Rasterizer).getColumns();
}

void Envelope3D::init() {
    interactor3D = &getObj(EnvelopeInter3D);
}
