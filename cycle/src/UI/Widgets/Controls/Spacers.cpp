#include "Spacers.h"

Separator::Separator(bool layout)
	:	isHorz(layout)
	,	x(0)
	, 	y(0) {
}

int Separator::getExpandedSize() {
    return 2;
}

int Separator::getCollapsedSize() {
    return 2;
}

void Separator::paint(Graphics& g) {
    // spacers
    ColourGradient gradient;
    gradient.point1 = Point<float>(0, 0);
    gradient.point2 = isHorz ? Point<float>(0, getHeight() - 2) : Point<float>(getWidth() - 2, 0);
    gradient.isRadial = false;

    gradient.addColour(0, Colour::greyLevel(0.08f));
    gradient.addColour(0.5f, Colour::greyLevel(0.16f));
    gradient.addColour(1.f, Colour::greyLevel(0.08f));

    g.setGradientFill(gradient);

    isHorz ? g.drawVerticalLine(0, 0, (float) getHeight()) : g.drawHorizontalLine(0, 0, (float) getWidth());

    g.setColour(Colour::greyLevel(0.06f));

    isHorz ? g.drawVerticalLine(1, 0, (float) getHeight()) : g.drawHorizontalLine(1, 0, (float) getWidth());
}

void Separator::setBoundsDelegate(int x, int y, int w, int h) {
    setBounds(x, y, w, h);
}

const Rectangle<int> Separator::getBoundsInParentDelegate() {
    return Rectangle<int>(getX(), getY(), isHorz ? 2 : 24, isHorz ? 24 : 2);
}

int Separator::getYDelegate() {
    return getY();
}

int Separator::getXDelegate() {
    return getX();
}
