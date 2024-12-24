#include "AmaranthLookAndFeel.h"

#include <utility>
#include <Util/Util.h>

#include "MiscGraphics.h"
#include "../App/Settings.h"
#include "../App/SingletonRepo.h"
#include "../UI/IConsole.h"
#include "../Definitions.h"

AmaranthLookAndFeel::AmaranthLookAndFeel(SingletonRepo* repo) :
        SingletonAccessor(repo, "AmaranthLookAndFeel") {
    Colour textColour(Colour::greyLevel(0.7f));
    Colour darkGrey(Colour::greyLevel(0.13f));

    setColour(CaretComponent::caretColourId, textColour);
    setColour(FileChooserDialogBox::titleTextColourId, textColour);
    setColour(MidiKeyboardComponent::whiteNoteColourId, Colours::black);
    setColour(ResizableWindow::backgroundColourId, Colours::black);
    setColour(DocumentWindow::textColourId, Colour::greyLevel(0.65f));

    setColour(ComboBox::backgroundColourId, darkGrey);
    setColour(ComboBox::textColourId, textColour);
    setColour(ComboBox::arrowColourId, textColour);
    setColour(ComboBox::buttonColourId, darkGrey);
    setColour(ComboBox::outlineColourId, Colours::black);

    setColour(Label::textColourId, textColour);

    setColour(PopupMenu::backgroundColourId, Colours::transparentBlack);
    setColour(PopupMenu::textColourId, textColour);
    setColour(PopupMenu::headerTextColourId, Colours::grey);
    setColour(PopupMenu::highlightedBackgroundColourId, Colours::black.withAlpha(0.7f));

    setColour(AlertWindow::backgroundColourId, darkGrey);
    setColour(AlertWindow::textColourId, textColour);
    setColour(AlertWindow::outlineColourId, Colour::greyLevel(0.3));

    setColour(TextEditor::focusedOutlineColourId, Colours::orange);
    setColour(TextEditor::textColourId, textColour);
    setColour(TextEditor::highlightColourId, Colours::grey);
    setColour(TextEditor::outlineColourId, Colour(130, 150, 180));
    setColour(TextEditor::backgroundColourId, darkGrey);
    setColour(TextButton::buttonColourId, Colours::grey);

    setColour(ListBox::textColourId, textColour);
    setColour(ListBox::backgroundColourId, Colours::transparentBlack);

    setDefaultLookAndFeel(this);
}

void AmaranthLookAndFeel::init() {
    arrowImg = getObj(MiscGraphics).getIcon(7, 6);
}

void AmaranthLookAndFeel::drawScrollbar(
        Graphics& g,
        ScrollBar& scrollbar,
        int xi, int yi, int width, int height,
        bool isScrollbarVertical,
        int thumbStartPosition,
        int thumbSize, bool /*isMouseOver*/, bool /*isMouseDown*/) {
    g.fillAll(Colours::black);
//	g.setColour(Colour::greyLevel(0.07f));

    Path slotPath, thumbPath;

    float x = xi;
    float y = yi;

    const float slotIndent = 0.f; // jmin(width, height) > 15 ? 1.0f : 0.0f;
    const float slotIndentx2 = slotIndent * 2.0f;
    const float thumbIndent = slotIndent; // + 1.0f;
    float thumbIndentx2 = thumbIndent;

    if (width > 8 && width < height)
        thumbIndentx2 = thumbIndent + 1;

    float gx1 = 0.0f, gy1 = 0.0f, gx2 = 0.0f, gy2 = 0.0f;

    if (isScrollbarVertical) {
        slotPath.addRectangle(x + slotIndent, y + slotIndent,
                              width - slotIndentx2, height - slotIndentx2);

        if (thumbSize > 0) {
            thumbPath.addRectangle(x + thumbIndent + 0.5f, thumbStartPosition + thumbIndent - 0.5f,
                                   width - thumbIndentx2, thumbSize - thumbIndentx2 + 1.f);
        }

        gx1 = (float) x;
        gx2 = x + width;
    } else {
        slotPath.addRectangle(x + slotIndent, y + slotIndent, width - slotIndentx2, height - slotIndentx2);

        if (thumbSize > 0)
            thumbPath.addRectangle(thumbStartPosition + thumbIndent - 0.5f, y + thumbIndent + 0.5f,
                                   thumbSize - thumbIndentx2 + 1.f, height - thumbIndentx2);
        gy1 = (float) y;
        gy2 = y + height;
    }

    g.setColour(Colour::greyLevel(0.09f));
    g.fillPath(slotPath);

    g.setColour(Colour::greyLevel(0.23f));
    g.fillPath(thumbPath);

    g.setColour(Colours::black);
    g.strokePath(thumbPath, PathStrokeType(1.f));
}

void AmaranthLookAndFeel::drawScrollbarButton(
        Graphics& g, ScrollBar& scrollbar,
        int width, int height, int buttonDirection,
        bool isScrollbarVertical, bool isMouseOverButton,
        bool isButtonDown) {
    enum Directions {
        VertUp, HorzDown, VertDown, HorzUp
    };

    bool isVertDown = buttonDirection == VertDown;
    g.fillAll(Colour::greyLevel(0.23f));
    g.setColour(Colour::greyLevel(0.09f));
    g.drawRect(0, isVertDown ? -1 : 0, width, height + (isVertDown ? 1 : 0), 1);

    Path p;
    Colour triangleColour(Colour::greyLevel(0.35f));

    if (buttonDirection == VertUp)
        p.addTriangle(
                width * 0.5f, height * 0.2f, width * 0.1f,
                height * 0.7f, width * 0.9f, height * 0.7f);
    else if (buttonDirection == HorzDown)
        p.addTriangle(
                width * 0.6f, height * 0.5f, width * 0.2f,
                height * 0.2f, width * 0.2f, height * 0.8f);
    else if (buttonDirection == VertDown)
        p.addTriangle(
                width * 0.5f, height * 0.8f, width * 0.1f,
                height * 0.3f, width * 0.9f, height * 0.3f);
    else if (buttonDirection == HorzUp)
        p.addTriangle(
                width * 0.3f, height * 0.5f, width * 0.7f,
                height * 0.2f, width * 0.7f, height * 0.8f);

    p.scaleToFit(3, height / 2 - 3, width - 6, 6, false);
    g.setColour(isButtonDown ? triangleColour.contrasting(0.2f) : triangleColour);
    g.fillPath(p);

    g.setColour(Colour(0x80000000));
    g.strokePath(p, PathStrokeType(0.5f));
}

void AmaranthLookAndFeel::drawFileBrowserRow(
        Graphics& g, int width, int height,
        const String& filename, Image* icon,
        const String& fileSizeDescription,
        const String& fileTimeDescription,
        const bool isDirectory, const bool isItemSelected, const int itemIndex,
        DirectoryContentsDisplayComponent&) {
    ColourGradient gradient;
    gradient.isRadial = false;
    gradient.point1 = Point<float>(0, 0);
    gradient.point2 = Point<float>(0, height);

    bool isOdd = itemIndex & 1;
    float selectedLevel = 0.3f;
    float oddLevel = isDirectory ? 0.25f : 0.15f;
    float evenLevel = isDirectory ? 0.2f : 0.12f;
    float middleDiff = isItemSelected ? -0.03f : isOdd ? 0.01f : -0.01f;

    float brightness = isItemSelected ? selectedLevel : isOdd ? oddLevel : evenLevel;
    float saturation = isDirectory ? 0.5f : 0.f;
    float hue = 0.1f;

    gradient.addColour(0.0f, Colour::fromHSV(hue, saturation, brightness, 1.f));
    gradient.addColour(0.5f, Colour::fromHSV(hue, saturation, brightness + middleDiff, 1.f));
    gradient.addColour(1.0f, Colour::fromHSV(hue, saturation, brightness, 1.f));

    g.setGradientFill(gradient);
    g.fillAll();

    const int x = 15;
    g.setColour(Colour::greyLevel(0.65f));
    g.setFont(height * 0.7f);

    if (width > 450 && !isDirectory) {
        const int sizeX = roundToInt(width * 0.7f);
        const int dateX = roundToInt(width * 0.8f);

        getObj(MiscGraphics).drawCentredText(g, Rectangle<int>(x, 0, sizeX - x, height), filename,
                                             Justification::centredLeft);
//		g.drawFittedText(filename, x, 0, sizeX - x, height, Justification::centredLeft, 1);
        g.setFont(*getObj(MiscGraphics).getSilkscreen());
        g.setColour(Colours::grey);

        if (!isDirectory) {
            g.drawFittedText(
                    fileSizeDescription,
                    sizeX,
                    0,
                    dateX - sizeX - 8,
                    height,
                    Justification::centredRight,
                    1);

            g.drawFittedText(
                    fileTimeDescription,
                    dateX,
                    0,
                    width - 8 - dateX,
                    height,
                    Justification::centredRight,
                    1);
        }
    } else {
        g.drawFittedText(filename, x, 0, width - x, height, Justification::centredLeft, 1);
    }
}

void AmaranthLookAndFeel::drawButtonBackground(Graphics& g,
                                               Button& button,
                                               const Colour& backgroundColour,
                                               bool isMouseOverButton,
                                               bool isButtonDown) {

    Colour deepRed(0xff200a15);

    float width = button.getWidth();
    float height = button.getHeight();
    Rectangle<int> fr(1, 1, width - 1, height - 1);

    Colour colour;

    bool isInactive = button.getProperties().contains("inactive");

    colour = isMouseOverButton ? (isInactive ? Colour::greyLevel(0.17f) : deepRed) : Colour::greyLevel(0.13f);

    g.setColour(colour);
    g.fillRect(fr);

    g.setColour(Colour::greyLevel(!isInactive && button.hasKeyboardFocus(true) ? 0.8f : 0.4f));

    Path path;
    path.addRoundedRectangle(fr.expanded(-1, -1).toFloat(), 1.f);
    g.strokePath(path, PathStrokeType(1.f),
                 AffineTransform::translation(-0.5f, -0.5f));

    g.setColour(Colours::black);
    Path path2;
    path2.addRoundedRectangle(fr.toFloat(), 1.f);
    g.strokePath(path2, PathStrokeType(1.f), AffineTransform::translation(-0.5f, -0.5f));
}

void AmaranthLookAndFeel::drawButtonText(Graphics& g,
                                         TextButton& button,
                                         bool isMouseOverButton,
                                         bool isButtonDown) {
    g.setFont(Font(FontOptions(16)));
    g.setColour(button.isEnabled() ? Colours::lightgrey : Colours::grey);
    g.drawFittedText(button.getButtonText(), 0, 0, button.getWidth(),
                     button.getHeight(), Justification::centred, 1);
}

void AmaranthLookAndFeel::drawGlassLozenge(Graphics& g,
                                           float x, float y,
                                           float width, float height,
                                           const Colour& colour,
                                           float outlineThickness,
                                           float cornerSize,
                                           bool flatOnLeft, bool flatOnRight,
                                           bool flatOnTop, bool flatOnBottom) noexcept {

    if (width <= outlineThickness || height <= outlineThickness) {
        return;
    }

    const float cs = cornerSize < 0 ? jmin(width * 0.3f, height * 0.3f) : cornerSize;

    Path outline;
    createRoundedPath(outline, x, y, width, height, cs,
                      !(flatOnLeft || flatOnTop),
                      !(flatOnRight || flatOnTop),
                      !(flatOnLeft || flatOnBottom),
                      !(flatOnRight || flatOnBottom));

    g.setColour(colour);
    g.fillPath(outline);

    g.setColour(Colour::greyLevel(0.64f));
    g.strokePath(outline, PathStrokeType(outlineThickness));
}

void AmaranthLookAndFeel::createRoundedPath(Path& p,
                                            const float x, const float y,
                                            const float w, const float h,
                                            const float cs,
                                            const bool curveTopLeft, const bool curveTopRight,
                                            const bool curveBottomLeft, const bool curveBottomRight) noexcept {
    const float cs2 = 2.0f * cs;
    const float pi = MathConstants<float>::pi;

    if (curveTopLeft) {
        p.startNewSubPath(x, y + cs);
        p.addArc(x, y, cs2, cs2, pi * 1.5f, pi * 2.0f);
    } else {
        p.startNewSubPath(x, y);
    }

    if (curveTopRight) {
        p.lineTo(x + w - cs, y);
        p.addArc(x + w - cs2, y, cs2, cs2, 0.0f, pi * 0.5f);
    } else {
        p.lineTo(x + w, y);
    }

    if (curveBottomRight) {
        p.lineTo(x + w, y + h - cs);
        p.addArc(x + w - cs2, y + h - cs2, cs2, cs2, pi * 0.5f, pi);
    } else {
        p.lineTo(x + w, y + h);
    }

    if (curveBottomLeft) {
        p.lineTo(x + cs, y + h);
        p.addArc(x, y + h - cs2, cs2, cs2, pi, pi * 1.5f);
    } else {
        p.lineTo(x, y + h);
    }

    p.closeSubPath();
}

class DocumentWindowButton :
        public Button,
        public SingletonAccessor {
public:
    DocumentWindowButton(SingletonRepo* repo,
                         const String& name, const Colour& col,
                         Path normalShape_,
                         Path toggledShape_) noexcept
            : Button(name),
              SingletonAccessor(repo, name),
              colour(col),
              normalShape(std::move(normalShape_)),
              toggledShape(std::move(toggledShape_)) {
    }

    ~DocumentWindowButton() override = default;

    void paintButton(Graphics& g, bool isMouseOverButton, bool isButtonDown) override {
        float alpha = isMouseOverButton ? (isButtonDown ? 1.0f : 0.8f) : 0.55f;

        if (!isEnabled()) {
            alpha *= 0.5f;
        }

        Rectangle<float> r(1, 0, getWidth() - 2, getHeight());

        g.setGradientFill(ColourGradient(colour.brighter(0.2f).withAlpha(alpha), r.getX() + 5, r.getY() + 5,
                                         colour.withAlpha(alpha), r.getX() + 26, r.getY() + 20, true));
        g.fillRoundedRectangle(r, 1.f);
        g.setColour(colour.brighter(0.1f));
        g.drawRoundedRectangle(r.translated(0.5f, 0.f), 2.f, 1.f);

        Path& p = getToggleState() ? toggledShape : normalShape;

        const AffineTransform t(p.getTransformToScaleToFit(8, 6, getWidth() - 14, getHeight() - 12, true));

        g.setColour(Colours::black.withAlpha(alpha * 0.9f));
        g.fillPath(p, t);
    }

    void mouseEnter(const MouseEvent& e) override {
        Button::mouseEnter(e);

        repo->getConsole().write(getButtonText());
        repo->getConsole().setMouseUsage(true, false, false, true);
    }

    juce_UseDebuggingNewOperator;

private:
    Colour colour;
    Path normalShape, toggledShape;

    DocumentWindowButton(const DocumentWindowButton&) = delete;
    DocumentWindowButton& operator=(const DocumentWindowButton&);
};


Button* AmaranthLookAndFeel::createDocumentWindowButton(int buttonType) {
    Path shape;
    const float crossThickness = 0.25f;

    if (buttonType == DocumentWindow::closeButton) {
        shape.addLineSegment(Line(0.0f, 0.0f, 1.0f, 1.0f), crossThickness * 1.4f);
        shape.addLineSegment(Line(1.0f, 0.0f, 0.0f, 1.0f), crossThickness * 1.4f);

        return new DocumentWindowButton(repo, "Close", Colour(0xff7d3524), shape, shape);
    }

    if (buttonType == DocumentWindow::minimiseButton) {
        shape.addLineSegment(Line(0.0f, 0.5f, 1.0f, 0.5f), crossThickness);

        return new DocumentWindowButton(repo, "Minimise", Colour(0xff2a2a2a), shape, shape);
    }

    if (buttonType == DocumentWindow::maximiseButton) {
        shape.addLineSegment(Line(0.5f, 0.0f, 0.5f, 1.0f), crossThickness);
        shape.addLineSegment(Line(0.0f, 0.5f, 1.0f, 0.5f), crossThickness);

        Path fullscreenShape;
        fullscreenShape.startNewSubPath(45.0f, 100.0f);
        fullscreenShape.lineTo(0.0f, 100.0f);
        fullscreenShape.lineTo(0.0f, 0.0f);
        fullscreenShape.lineTo(100.0f, 0.0f);
        fullscreenShape.lineTo(100.0f, 45.0f);
        fullscreenShape.addRectangle(45.0f, 45.0f, 100.0f, 100.0f);
        PathStrokeType(30.0f).createStrokedPath(fullscreenShape, fullscreenShape);

        return new DocumentWindowButton(repo, "Cycle Window Size", Colour(0xff7f5506), shape, fullscreenShape);
    }

    jassertfalse;
    return nullptr;
}

void AmaranthLookAndFeel::positionDocumentWindowButtons(DocumentWindow&,
                                                        int titleBarX,
                                                        int titleBarY,
                                                        int titleBarW,
                                                        int titleBarH,
                                                        Button* minimiseButton,
                                                        Button* maximiseButton,
                                                        Button* closeButton,
                                                        bool positionTitleBarButtonsOnLeft) {
    const int buttonW = 24.f;
    const int closeButtonW = 35.f;

    int x = positionTitleBarButtonsOnLeft
                ? titleBarX
                : titleBarX + titleBarW - closeButtonW;

    if (closeButton != nullptr) {
        closeButton->setBounds(x, titleBarY, closeButtonW, titleBarH);
        x += positionTitleBarButtonsOnLeft ? closeButtonW : -(buttonW); // + buttonW / 8
    }

    if (positionTitleBarButtonsOnLeft) {
        std::swap(minimiseButton, maximiseButton);
    }

    if (maximiseButton != nullptr) {
        maximiseButton->setBounds(x, titleBarY, buttonW, titleBarH);
        x += positionTitleBarButtonsOnLeft ? buttonW : -buttonW;
    }

    if (minimiseButton != nullptr) {
        minimiseButton->setBounds(x, titleBarY, buttonW, titleBarH);
    }
}

Button* AmaranthLookAndFeel::createFileBrowserGoUpButton() {
    auto* goUpButton = new DrawableButton("up", DrawableButton::ImageOnButtonBackground);

    Path arrowPath;
    arrowPath.addArrow(Line<float>(50.0f, 100.0f, 50.0f, 0.0f), 40.0f, 100.0f, 50.0f);

    DrawablePath arrowImage;
    arrowImage.setFill(Colour::greyLevel(0.64f));
    arrowImage.setPath(arrowPath);

    goUpButton->setImages(&arrowImage);

    return goUpButton;
}

void AmaranthLookAndFeel::fillResizableWindowBackground(Graphics& g, int w, int h,
                                                        const BorderSize<int>& border,
                                                        ResizableWindow& window) {
//	if(w > 700 || h > 700)
//	{
    g.fillAll(Colours::black);
//		return;
//	}
//
//	Image& bg = getObj(MiscGraphics).getImage(MiscGraphics::blackgroundImg);
//	g.drawImage(bg, 0, 0, w, h, 0, 0, bg.getWidth(), bg.getHeight(), false);
//	g.setColour(Colours::black);
//	g.drawRect(0, 0, w, h, 1);
}


void AmaranthLookAndFeel::drawTextEditorOutline(Graphics& g, int width, int height, TextEditor& textEditor) {
    if (textEditor.isEnabled()) {

        const bool focused = textEditor.hasKeyboardFocus(true) && !textEditor.isReadOnly();
        g.setColour(textEditor.findColour(focused ?
            TextEditor::focusedOutlineColourId :
            TextEditor::outlineColourId));
        g.drawRect(0, 0, width, height);
        g.setOpacity(1.0f);

        Colour shadowColour(textEditor.findColour(TextEditor::shadowColourId).withMultipliedAlpha(
            focused ? 0.75f : 1.f));

        drawBevelA(g, width, height, 3, shadowColour);
    }
}

void AmaranthLookAndFeel::drawLevelMeter(Graphics& g, int width, int height, float level) {
    g.setColour(Colour(0xff1F252a));
    g.fillRoundedRectangle(0.0f, 0.0f, (float) width, (float) height, 3.0f);
    g.setColour(Colours::black.withAlpha(0.2f));
    g.drawRoundedRectangle(1.0f, 1.0f, width - 2.0f, height - 2.0f, 3.0f, 1.0f);

    const int totalBlocks = 20;
    const int numBlocks = roundToInt(totalBlocks * level);
    const float w = (width - 6.0f) / (float) totalBlocks;

    for (int i = 0; i < totalBlocks; ++i) {
        if (i >= numBlocks) {
            g.setColour(Colour(20, 22, 25));
        } else {
            g.setColour(i < totalBlocks - 1 ? Colours::blue.withSaturation(0.3f) : Colours::red);
        }

        g.fillRect(3.0f + i * w + w * 0.1f, 3.0f, w * 0.8f, height - 6.0f);
    }
}

int AmaranthLookAndFeel::getDefaultMenuBarHeight() {
    return 25;
}

void AmaranthLookAndFeel::drawMenuBarBackground(Graphics& g,
                                                int width, int height, bool isMouseOverBar,
                                                MenuBarComponent& menuBar) {
    g.fillAll(Colour::greyLevel(0.1f));
//	Image& bg = getObj(MiscGraphics).getImage(MiscGraphics::blackgroundImg);
//	g.drawImage(bg, 0, 0, width, height, 0, 0, bg.getWidth(), bg.getHeight?());
}

Font AmaranthLookAndFeel::getMenuBarFont(MenuBarComponent& menuBar, int itemIndex, const String& itemText) {
    return FontOptions(15);
}

void AmaranthLookAndFeel::drawPopupMenuBackground(Graphics& g, int width, int height) {
    float alpha = getSetting(LastPopupClickedTransp) ? 0.5f : 1.f;

    const Colour background(Colour::greyLevel(0.1f).withAlpha(alpha));
    g.fillAll(background);

    g.setColour(Colour::greyLevel(0.06f).withAlpha(alpha));
    for (int i = 0; i < height; i += 2) {
        g.drawHorizontalLine(i, 0, width);
    }

    bool horz = getSetting(LastPopupClickedHorz) == 1;

    int w = horz ? width : 0;
    int h = horz ? 0 : height;

    g.setGradientFill(ColourGradient(Colours::darkgrey, w, h, Colour::greyLevel(0.08f), 0, 0, false));
    g.drawRect(0, 0, width, height);
}

void AmaranthLookAndFeel::drawTableHeaderBackground(Graphics& g, TableHeaderComponent& header) {
    g.fillAll(Colour::greyLevel(0.08));
}

void AmaranthLookAndFeel::drawTableHeaderColumn(Graphics& g, const String& columnName, int /*columnId*/,
                                                int width, int height, bool isMouseOver, bool isMouseDown,
                                                int columnFlags) {
    float level = isMouseDown ? 0.3f : isMouseOver ? 0.4f : 0.25f;
    g.setColour(Colour::fromHSV(0.6f, 0.4f, level, 1.f));
    g.fillRect(0, 0, width - 1, height - 1);

    int rightOfText = width - 4;

    if ((columnFlags & (TableHeaderComponent::sortedForwards | TableHeaderComponent::sortedBackwards)) != 0) {
        const float top = height * ((columnFlags & TableHeaderComponent::sortedForwards) != 0 ? 0.4f : (1.0f - 0.4f));
        const float bottom = height - top;
        const float w = height * 0.5f;
        const float x = rightOfText - (w * 1.25f);

        Path sortArrow;
        sortArrow.addTriangle(x, bottom, x + w * 0.5f, top, x + w, bottom);

        g.setColour(Colour::greyLevel(0.64f));
        g.fillPath(sortArrow);
    }

    g.setColour(Colour::greyLevel(0.64f));
    g.setFont(height * 0.5f + 1);

    const int textX = 4;

    getObj(MiscGraphics).drawShadowedText(g, columnName, textX, 3 * height / 4 - 2, g.getCurrentFont(), 0.65f);
}

void AmaranthLookAndFeel::drawPopupMenuItem(Graphics& g,
                                            const Rectangle<int>& area,
                                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                            bool hasSubMenu,
                                            const String& text, const String& shortcutKeyText,
                                            const Drawable* image,
                                            const Colour* textColourToUse) {
    if (isSeparator) {
        Rectangle r(area.reduced(5, 0));
        r.removeFromTop(r.getHeight() / 2 - 1);

        g.setColour(Colour(0x33000000));
        g.fillRect(r.removeFromTop(1));

        g.setColour(Colour(0x66ffffff));
        g.fillRect(r.removeFromTop(1));
    } else {
        Colour textColour(findColour(PopupMenu::textColourId));

        if (textColourToUse != nullptr) {
            textColour = *textColourToUse;
        }

        Rectangle r(area.reduced(1, 1));

        if (isHighlighted) {
            g.setColour(findColour(PopupMenu::highlightedBackgroundColourId));
            g.fillRect(r);

            g.setColour(findColour(PopupMenu::highlightedTextColourId));
        } else {
            g.setColour(textColour);
        }

        if (!isActive) {
            g.setOpacity(0.3f);
        }

        Font font(getPopupMenuFont());

        const float maxFontHeight = area.getHeight() / 1.3f;

        if (font.getHeight() > maxFontHeight)
            font.setHeight(maxFontHeight);

        g.setFont(font);

//		Rectangle<float> iconArea (r.removeFromLeft ((r.getHeight() * 5) / 4).reduced (3).toFloat());
        Rectangle<float> iconArea(1, (r.getHeight() - 24) / 2, 24, 24);
        r.removeFromLeft(r.getHeight() * 5 / 4);

        if (image != nullptr) {
            image->drawWithin(g, iconArea, RectanglePlacement::centred | RectanglePlacement::doNotResize, 1.0f);
        } else if (isTicked) {
            iconArea = iconArea.reduced(2, 5);
            const Path tick(getTickShape(jmin(iconArea.getWidth(), iconArea.getHeight())));

            g.fillPath(tick, AffineTransform());
//					tick.getTransformToScaleToFit(iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true));
        }

        if (hasSubMenu) {
            const float arrowH = 0.6f * getPopupMenuFont().getAscent();

            const float x = (float) r.removeFromRight((int) arrowH).getX();
            const float halfH = (float) r.getCentreY();

            Path p;
            p.addTriangle(x, halfH - arrowH * 0.5f,
                          x, halfH + arrowH * 0.5f,
                          x + arrowH * 0.6f, halfH);

            g.fillPath(p);
        }

        r.removeFromRight(3);
        getObj(MiscGraphics).drawCentredText(g, r, text, Justification::centredLeft);

        if (shortcutKeyText.isNotEmpty()) {
            Font f2(font);
            f2.setHeight(f2.getHeight() * 0.75f);
            f2.setHorizontalScale(0.95f);

            g.setFont(f2);
            getObj(MiscGraphics).drawCentredText(g, r, shortcutKeyText, Justification::centredRight);
        }

    }
}

//==============================================================================
void AmaranthLookAndFeel::drawComboBox(Graphics& g, int width, int height,
                                       const bool isButtonDown,
                                       int buttonX, int buttonY,
                                       int buttonW, int buttonH,
                                       ComboBox& box) {
    g.fillAll(box.findColour(ComboBox::backgroundColourId));

    //const float outlineThickness = box.isEnabled() ? (isButtonDown ? 1.2f : 0.5f) : 0.3f;

    g.setColour(Colour::greyLevel(0.5f));

    int imageX = width - arrowImg.getWidth() - 3;
    int imageY = height - arrowImg.getHeight() - 4;

    g.setOpacity(0.7f);
    g.drawImageAt(arrowImg, imageX, imageY);
    g.setOpacity(1.f);
    g.setColour(box.findColour(ComboBox::outlineColourId));

    getObj(MiscGraphics).drawCorneredRectangle(g, Rectangle<int>(0, 0, width - 1, height - 1));
}

void AmaranthLookAndFeel::positionComboBoxText(ComboBox& box, Label& label) {
    if (box.getWidth() < 80) {
        label.setBounds(0, 2,
                        box.getWidth() - 3,
                        box.getHeight() - 2);
    } else {
        label.setBounds(1, 1,
                        box.getWidth() + 3 - box.getHeight(),
                        box.getHeight() - 2);

    }

    label.setFont(getComboBoxFont(box));
}


int AmaranthLookAndFeel::getMenuWindowFlags() {
    return 0;
}

void AmaranthLookAndFeel::drawAlertBox(Graphics& g, AlertWindow& alert, const Rectangle<int>& textArea,
                                       TextLayout& textLayout) {
//	getObj(MiscGraphics).fillBlackground(&alert, g);
//	g.fillAll (alert.findColour (AlertWindow::backgroundColourId));

    int iconSpaceUsed = 0;
    Justification alignment(Justification::horizontallyCentred);

    const int iconWidth = 30;
    int iconSize = jmin(iconWidth + 20, alert.getHeight() + 10);

    if (alert.containsAnyExtraComponents() || alert.getNumButtons() > 2)
        iconSize = jmin(iconSize, textArea.getHeight() + 50);

    const Rectangle iconRect(30, 30, iconSize, iconSize);

    if (alert.getAlertType() != AlertWindow::NoIcon) {
        Path icon;
        uint32 colour;
        char character;

        if (alert.getAlertType() == AlertWindow::WarningIcon) {
            colour = 0x55ff5555;
            character = '!';

            icon.addTriangle(iconRect.getX() + iconRect.getWidth() * 0.5f, (float) iconRect.getY(),
                             (float) iconRect.getRight(), (float) iconRect.getBottom(),
                             (float) iconRect.getX(), (float) iconRect.getBottom());

            icon = icon.createPathWithRoundedCorners(5.0f);
        } else {
            colour = alert.getAlertType() == AlertWindow::InfoIcon ? 0x605555ff : 0x40b69900;
            character = alert.getAlertType() == AlertWindow::InfoIcon ? 'i' : '?';

            icon.addEllipse((float) iconRect.getX(), (float) iconRect.getY(),
                            (float) iconRect.getWidth(), (float) iconRect.getHeight());
        }

        GlyphArrangement ga;
        ga.addFittedText(Font(FontOptions(iconRect.getHeight() * 0.9f, Font::bold)),
                         String::charToString(character),
                         (float) iconRect.getX(), (float) iconRect.getY(),
                         (float) iconRect.getWidth(), (float) iconRect.getHeight(),
                         Justification::centred, false);
        ga.createPath(icon);

        icon.setUsingNonZeroWinding(false);
        g.setColour(Colour(colour));
        g.fillPath(icon);

        iconSpaceUsed = iconWidth * 3;
        alignment = Justification::left;
    }

    g.setColour(alert.findColour(AlertWindow::textColourId));

    Rectangle<float> r = textArea.toFloat();
    r.removeFromLeft(iconSpaceUsed);
    textLayout.draw(g, r);

    g.setColour(alert.findColour(AlertWindow::outlineColourId));
    g.drawRect(0, 0, alert.getWidth(), alert.getHeight());
}

void AmaranthLookAndFeel::drawLabel(Graphics& g, Label& label) {
    Colour textColour(Colour::greyLevel(0.64f));

    label.setColour(TextEditor::textColourId, textColour);

    g.fillAll(label.findColour(Label::backgroundColourId));

    if (!label.isBeingEdited()) {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const Font font(getLabelFont(label));

        g.setColour(label.findColour(Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(font);

        Rectangle textArea(label.getBorderSize().subtractedFrom(label.getLocalBounds()));

        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         jmax(1, (int) (textArea.getHeight() / font.getHeight())),
                         label.getMinimumHorizontalScale());

        g.setColour(label.findColour(Label::outlineColourId).withMultipliedAlpha(alpha));
        g.drawRect(0, 0, label.getWidth(), label.getHeight());
    } else if (label.isEnabled()) {
        label.setColour(Label::textColourId, textColour);
        g.setColour(textColour);
        g.drawRect(0, 0, label.getWidth(), label.getHeight());
    }
}

void AmaranthLookAndFeel::drawCornerResizer(Graphics& g,
                                            int w, int h,
                                            bool /*isMouseOver*/,
                                            bool /*isMouseDragging*/) {
    const float lineThickness = jmin(w, h) * 0.075f;

    for (float i = 0.0f; i < 1.0f; i += 0.3f) {
        g.setColour(Colours::lightgrey);

        g.drawLine(w * i,
                   h + 1.0f,
                   w + 1.0f,
                   h * i,
                   lineThickness);

        g.setColour(Colours::darkgrey);

        g.drawLine(w * i + lineThickness,
                   h + 1.0f,
                   w + 1.0f,
                   h * i + lineThickness,
                   lineThickness);
    }
}

void AmaranthLookAndFeel::drawTickBox(Graphics& g, Component& component,
                                      float x, float y, float w, float h,
                                      bool ticked, bool isEnabled,
                                      bool isMouseOverButton, bool isButtonDown) {
    const float boxSize = 2 * int((w) * 0.5f) - 5;
    Rectangle<int> boxRect(x - 1, y, boxSize, boxSize);
    Rectangle<float> floatRect = boxRect.toFloat().translated(-0.5, 2.5f);

    g.setColour(Colour::greyLevel(0.64f));
    g.drawRoundedRectangle(floatRect, 2.f, 1.f);

    if (ticked) {
        g.fillRect(floatRect.getSmallestIntegerContainer().reduced(2, 2));
    }
}

void AmaranthLookAndFeel::drawBevelA(Graphics& g, int width, int height, int border, const Colour& shadowColour) {
    drawBevel(g, 0, 0, width, height, border, shadowColour, shadowColour, false, true);
}

void AmaranthLookAndFeel::drawCallOutBoxBackground(CallOutBox& box, Graphics& g,
                                                   const Path& path, Image& cachedImage) {
    drawCallOutBoxBackground(g, path);
}

void AmaranthLookAndFeel::drawCallOutBoxBackground(Graphics& g, const Path& path) {
    g.setColour(Colour::greyLevel(0.13f).withAlpha(0.8f));
    g.fillPath(path);

    g.setColour(Colours::grey.withAlpha(0.8f));
    g.strokePath(path, PathStrokeType(1.0f));
}

void AmaranthLookAndFeel::layoutFileBrowserComponent(FileBrowserComponent& browserComp,
                                                     DirectoryContentsDisplayComponent* fileListComponent,
                                                     FilePreviewComponent* previewComp,
                                                     ComboBox* currentPathBox,
                                                     TextEditor* filenameBox,
                                                     Button* goUpButton) {
    LookAndFeel_V2::layoutFileBrowserComponent(browserComp, fileListComponent, previewComp, currentPathBox, filenameBox,
                                               goUpButton);
}

int AmaranthLookAndFeel::getMenuBarItemWidth(MenuBarComponent& menuBar, int itemIndex, const String& itemText) {
    return Util::getStringWidth(getMenuBarFont(menuBar, itemIndex, itemText), itemText) + menuBar.getHeight() -
           (menuBar.getWidth() <= 250 ? menuBar.getHeight() / 2 : 0);
}

Path AmaranthLookAndFeel::getTickShape(float height) {
    float half = height * 0.5f;
    float width = half * 0.3f;

    Path path;
    path.addEllipse(height, height * 0.7, width * 2, width * 2);

    return path;
}
