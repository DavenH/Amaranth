#include "MiscGraphics.h"

#include <Definitions.h>
#include <App/SingletonRepo.h>
#include <Util/Util.h>

#include "Widgets/IconButton.h"
#include "../Binary/Images.h"
#include "../Binary/Silkscreen.h"
#include "../Util/NumberUtils.h"
#include "../Util/CommonEnums.h"
#include "../App/AppConstants.h"

MiscGraphics::MiscGraphics(SingletonRepo* repo) : SingletonAccessor(repo, "MiscGraphics") {
}

void MiscGraphics::init() {
    icons = PNGImageFormat::loadFrom(Images::icons_png, Images::icons_pngSize);
    jassert(! icons.isNull());

    powerIcon = getIcon(5, 5);
    powerIcon.duplicateIfShared();

    cursors.add(new MouseCursor(getIcon(1, 2), 0,  0));  // pencil
    cursors.add(new MouseCursor(getIcon(2, 2), 0,  0));  // pencil edit
    cursors.add(new MouseCursor(getIcon(0, 2), 11, 11)); // cross cursor
    cursors.add(new MouseCursor(getIcon(5, 6), 11, 11)); // cross add cursor
    cursors.add(new MouseCursor(getIcon(4, 6), 11, 11)); // cross sub cursor
    cursors.add(new MouseCursor(getIcon(8, 7), 0,  0));  // cancel cursor

    // MemoryInputStream fontStream(Silkscreen::output, Silkscreen::outputSize, false);
    // auto typeface = Typeface::createSystemTypefaceFor(Silkscreen::output, Silkscreen::outputSize);
    // silkscreen = new Font(FontOptions(typeface));

    String fontface = getStrConstant(FontFace);

    silkscreen = new Font(FontOptions(fontface, 9.f, Font::plain));
    verdana12 = new Font(FontOptions(fontface, 12.f, Font::plain));
    verdana16 = new Font(FontOptions(fontface, 16.f, Font::plain));
    silkscreen->setHeight(7.4f);

    blueGlow.setGlowProperties(3, Colour(90, 180, 240));
    yllwGlow.setGlowProperties(3, Colour(240, 180, 0));
    redGlow.setGlowProperties(2, Colours::lightblue.withAlpha(0.5f));

    shadow.setShadowProperties(DropShadow(Colours::black, 2, Point(2, 1)));
    insetUp.setShadowProperties(DropShadow(Colours::black, 1.f, Point(-1, -1)));

    fonts.add(silkscreen);
    fonts.add(verdana12);
    fonts.add(verdana16);
}

void MiscGraphics::drawCorneredRectangle(Graphics& g, const Rectangle<int>& r, int cornerSize) {
    Path path;
    if (cornerSize) {
        path.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), cornerSize);
    } else {
        path.addRectangle(r);
    }

    g.strokePath(path, PathStrokeType(1.f), AffineTransform::translation(0.5f, 0.5f));
}

const MouseCursor& MiscGraphics::getCursor(int cursor) const {
    if (NumberUtils::within<int>(cursor, 0, NudgeCursorHorz)) {
        return *cursors[cursor];
    }

    if (extension != nullptr) {
        return extension->getCursor(cursor);
    }

    return *cursors[CrossCursor];
}

void MiscGraphics::applyMouseoverHighlight(Graphics& g, Image copy, bool mouseOver, bool buttonDown, bool pending) {
    if (mouseOver && ! buttonDown) {
        redGlow.applyEffect(copy, g, 1.f, 0.5f);
    } else if (buttonDown) {
        yllwGlow.applyEffect(copy, g, 1.f, 0.7f);
    } else if (pending) {
        blueGlow.applyEffect(copy, g, 1.f, 0.7f);
    } else {
        shadow.applyEffect(copy, g, 1.f, 1.f);
    }
}

void MiscGraphics::drawShadowedText(Graphics& g, const String& text, int x, int y, const Font& font, float alpha) {
    g.setFont   (font);
    g.setColour (Colour::greyLevel(0.04f));
    g.drawSingleLineText(text, x + 1, y + 1);

    g.setColour (Colour::greyLevel(alpha));
    g.drawSingleLineText(text, x, y);
}

Font* MiscGraphics::getAppropriateFont(int scaleSize) {
    switch (scaleSize) {
        case ScaleSizes::ScaleSmall:    return silkscreen;
        case ScaleSizes::ScaleMed:      return verdana12;
        case ScaleSizes::ScaleLarge:    return verdana16;

        default: {
            if (extension != nullptr) {
                return extension->getAppropriateFont(scaleSize);
            }
        }
    }

    return silkscreen;
}

Image& MiscGraphics::getImage(int imageEnum) {
    using namespace AppImages;

    if (imageEnum == iconsImg) {
        return icons;
    }

    if (extension != nullptr) {
        return extension->getImage(imageEnum);
    }
    throw std::out_of_range("MiscGraphics::getImage()");
}

void MiscGraphics::drawHighlight(Graphics& g, Component* c, int shiftX, int shiftY) {
    Rectangle<float> r(c->getX() + shiftX, c->getY() + shiftY, c->getWidth(), c->getHeight());
    drawHighlight(g, r);
}

void MiscGraphics::drawHighlight(Graphics& g, const Rectangle<float>& r) {
    g.setGradientFill(ColourGradient(Colour::greyLevel(0.7f), r.getX() + 5,
                                     r.getY() + 5, Colour(70, 84, 98),
                                     r.getX() + 26, r.getY() + 20, true));
    g.setOpacity(0.35f);
    g.fillRoundedRectangle(r, 2);
    g.setOpacity(1.f);
}

void MiscGraphics::addPulloutIcon(Image& image, bool horz) {
    Graphics g(image);

    g.setOpacity(1.f);
    g.drawImageAt(getIcon(0, 0), 24, 24, true);
}

Image MiscGraphics::getIcon(int x, int y) const {
    return icons.getClippedImage(Rectangle<int>(x * 24, y * 24, 24, 24));
}

void MiscGraphics::drawPowerSymbol(Graphics& g, Rectangle<int> bounds) const {
    g.drawImageWithin(powerIcon, bounds.getX(), bounds.getY(), bounds.getWidth(),
                      bounds.getHeight(), RectanglePlacement::centred);
}

void MiscGraphics::drawJustifiedText(Graphics& g, const String& text, const Rectangle<int>& rect,
                                     bool above, Component* parent) {
    int width = rect.getRight() - rect.getX();
    int strWidth = roundToInt(Util::getStringWidth(*silkscreen, text));

    int x = rect.getX() + (width - strWidth) / 2;
    int y = rect.getY() + 4;

    if (parent != nullptr) {
        int parentY = parent->getY();
        y = above ? (rect.getY() - 5 + parentY) : (rect.getBottom() + 8 + parentY);
        x += parent->getX();
    }

    drawShadowedText(g, text, x, y, *silkscreen);
}

void MiscGraphics::drawJustifiedText(Graphics& g, const String& text,
                                     Component& topLeft, Component& botRight,
                                     bool above, Component* parent) {
    drawJustifiedText(g, text,
                      Rectangle(topLeft.getBounds().getTopLeft(), botRight.getBounds().getBottomRight()),
                      above, parent);
}

