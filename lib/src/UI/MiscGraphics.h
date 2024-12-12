#pragma once

#include "JuceHeader.h"
#include "../App/SingletonAccessor.h"

class SingletonRepo;

namespace AppImages {
    enum {
		iconsImg,

		numImages
	};
}

using namespace juce;

class MiscGraphics : SingletonAccessor {
public:
    enum FontEnum {
		Silkscreen,
		Verdana12,
		Verdana16,
	};

    enum CursorType {
		PencilCursor,
		PencilEditCursor,
		CrossCursor,
		CrossAddCursor,
		CrossSubCursor,
		NotApplicableCursor,
		NudgeCursorVert,
		NudgeCursorHorz,

		numCursors
	};

	class Extension {
	public:
		virtual ~Extension() = default;

		virtual Image& getImage(int imageEnum) 				= 0;
		virtual Font* getAppropriateFont(int scaleSize) 	= 0;
		virtual const MouseCursor& getCursor(int cursor) 	= 0;
	};

	explicit MiscGraphics(SingletonRepo* repo);
	~MiscGraphics() override = default;

	Font* getAppropriateFont(int scaleSize);
	Font* getSilkscreen() 	{ return silkscreen; }
	Font* getVerdana12() 	{ return verdana12;  }
	Font* getVerdana16() 	{ return verdana16;  }

	Image getIcon(int x, int y) const;
	Image& getImage(int imageEnum);
	Image& getPowerIcon() { return powerIcon; }

	const MouseCursor& getCursor(int cursor) const;

	void addPulloutIcon(Image& image, bool horz = true);
	void applyMouseoverHighlight(Graphics& g, Image copy, bool mouseOver, bool buttonDown, bool pending);

	static void drawCorneredRectangle(Graphics& g, const Rectangle<int>& r, int cornerSize = 0);
	void drawHighlight(Graphics& g, Component* c, int shiftX = 0, int shiftY = 0);
	void drawHighlight(Graphics& g, const Rectangle<float>& r);
	void drawJustifiedText(Graphics& g, const String& text, Component& topLeft, Component& botRight, bool above, Component* parent = 0);
	void drawJustifiedText(Graphics& g, const String& text, const Rectangle<int>& rect, bool above = true, Component* parent = 0);
	void drawPowerSymbol(Graphics& g, Rectangle<int> bounds) const;
	static void drawShadowedText(Graphics& g, const String& text, int x, int y, const Font& font, float alpha = 0.65f);

	static void drawTransformedText(Graphics& g, const String& text, const AffineTransform& transform)
	{
		Graphics::ScopedSaveState sss(g);
		g.addTransform(transform);
		g.drawSingleLineText(text, 0, 0, Justification::left);
	}

	template<class T>
	void drawRoundedRectangle(Graphics& g, const Rectangle<T>& r, float cornerSize = 3.f)
	{
		Path path;
		path.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), cornerSize);
		g.strokePath(path, PathStrokeType(1.f), AffineTransform::translation(-0.5f, -0.5f));
	}

	static void drawCentredText(Graphics& g, const Rectangle<int>& r, const String& text, Justification j11n = Justification::centred)
	{
		g.drawText(text, r, j11n, false);
	}

	void setExtension(Extension* extension) { this->extension = extension; }

protected:
	Font* silkscreen;
	Font* verdana12;
	Font* verdana16;

	Extension* extension{};

	Image icons;
	Image powerIcon;

    GlowEffect blueGlow;
    GlowEffect yllwGlow;
    GlowEffect redGlow;

    DropShadowEffect shadow;
    DropShadowEffect insetUp;
    DropShadowEffect insetDown;

	OwnedArray<Font> fonts;
	OwnedArray<MouseCursor> cursors;

	friend class Extension;
};
