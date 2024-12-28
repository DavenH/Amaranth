#pragma once

#include "JuceHeader.h"
#include "../App/SingletonAccessor.h"

class AmaranthLookAndFeel :
        public SingletonAccessor
    ,   public LookAndFeel_V3 {
private:
    JUCE_LEAK_DETECTOR(AmaranthLookAndFeel);

    Image arrowImg;

public:
    explicit AmaranthLookAndFeel(SingletonRepo* repo);

    void init() override;

    void drawScrollbar(
            Graphics& g,
            ScrollBar& scrollbar,
            int x, int y,
            int width, int height,
            bool isScrollbarVertical,
            int thumbStartPosition,
            int thumbSize,
            bool /*isMouseOver*/,
            bool /*isMouseDown*/) override;

    void drawScrollbarButton(
            Graphics& g,
            ScrollBar& scrollbar,
            int width, int height,
            int buttonDirection,
            bool isScrollbarVertical,
            bool isMouseOverButton,
            bool isButtonDown) override;

    void drawFileBrowserRow(
            Graphics& g,
            int width, int height,
            const String& filename,
            Image* icon,
            const String& fileSizeDescription,
            const String& fileTimeDescription,
            bool isDirectory,
            bool isItemSelected,
            int /*itemIndex*/,
            DirectoryContentsDisplayComponent&);

    void drawGlassLozenge(
            Graphics& g,
            float x, float y,
            float width, float height,
            const Colour& colour,
            float outlineThickness,
            float cornerSize,
            bool flatOnLeft,
            bool flatOnRight,
            bool flatOnTop,
            bool flatOnBottom) noexcept;

    void createRoundedPath (
            Path& p,
            const float x, const float y,
            const float w, const float h,
            const float cs,
            const bool curveTopLeft, const bool curveTopRight,
            const bool curveBottomLeft, const bool curveBottomRight) noexcept;

    void drawButtonBackground (Graphics& g,
            Button& button,
            const Colour& backgroundColour,
            bool isMouseOverButton,
            bool isButtonDown) override;

    void drawButtonText (Graphics& g,
            TextButton& button,
            bool isMouseOverButton,
            bool isButtonDown) override;

    void positionDocumentWindowButtons (DocumentWindow&,
            int titleBarX,
            int titleBarY,
            int titleBarW,
            int titleBarH,
            Button* minimiseButton,
            Button* maximiseButton,
            Button* closeButton,
            bool positionTitleBarButtonsOnLeft) override;

    void fillResizableWindowBackground(
            Graphics& g,
            int w,
            int h,
            const BorderSize<int>& border,
            ResizableWindow& window) override;

    void drawTextEditorOutline (Graphics& g, int width, int height, TextEditor& textEditor) override;
    void drawLevelMeter (Graphics& g, int width, int height, float level) override;

    Button* createDocumentWindowButton (int buttonType) override;
    Button* createFileBrowserGoUpButton() override;

    int getDefaultMenuBarHeight() override;
    void drawMenuBarBackground (Graphics& g, int width, int height,
                                        bool isMouseOverBar,
                                        MenuBarComponent& menuBar) override;

    Font getMenuBarFont (MenuBarComponent& menuBar, int itemIndex, const String& itemText) override;
    int getMenuBarItemWidth (MenuBarComponent& menuBar, int itemIndex, const String& itemText) override;
    void drawPopupMenuBackground (Graphics& g, int width, int height) override;

    void drawPopupMenuItem(Graphics& g, const Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const String& text, const String& shortcutKeyText,
                           const Drawable* image, const Colour* textColour) override;

    void drawTableHeaderBackground (Graphics& g, TableHeaderComponent& header) override;

    void drawTableHeaderColumn (Graphics& g, const String& columnName, int columnId,
                                        int width, int height,
                                        bool isMouseOver, bool isMouseDown,
                                        int columnFlags);

    void drawComboBox (Graphics& g, int width, int height,
                                    bool isButtonDown,
                                    int buttonX, int buttonY,
                                    int buttonW, int buttonH,
                                    ComboBox& box) override;

    void positionComboBoxText (ComboBox& box, Label& labelToPosition) override;
    int getMenuWindowFlags() override;

    void drawAlertBox(Graphics& g, AlertWindow& alert, const Rectangle<int>& textArea, TextLayout& textLayout) override;

    void drawLabel (Graphics& g, Label& label) override;

    void drawCornerResizer (Graphics& g,
                            int w, int h,
                            bool /*isMouseOver*/,
                            bool /*isMouseDragging*/) override;


    void drawTickBox(Graphics& g, Component& component,
                     float x, float y, float w, float h,
                     bool ticked, bool isEnabled,
                     bool isMouseOverButton, bool isButtonDown) override;

    void drawBevelA(Graphics& g, int width, int height, int border, const Colour& shadowColour);

    bool areScrollbarButtonsVisible() override { return false; }

    void drawCallOutBoxBackground (CallOutBox&, Graphics&, const Path& path, Image& cachedImage) override;
    void drawCallOutBoxBackground (Graphics& g, const Path& path);

    void layoutFileBrowserComponent (FileBrowserComponent& browserComp,
                                     DirectoryContentsDisplayComponent* fileListComponent,
                                     FilePreviewComponent* previewComp,
                                     ComboBox* currentPathBox,
                                     TextEditor* filenameBox,
                                     Button* goUpButton) override;

    Path getTickShape (float height) override;
};
