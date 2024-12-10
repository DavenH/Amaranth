#pragma once
#include "JuceHeader.h"

using namespace juce;

class Texture {
public:
	Texture() = default;
	explicit Texture(Image& image) : image(image) {}
	virtual ~Texture() = default;

	Image image;
	Rectangle<float> rect;

	virtual void create() {}
	virtual void clear() {}
	virtual void bind() {}
};


class TextureGL : public Texture {
public:
	GLuint id;
	int blendFunc;
//	float scale;
	Rectangle<float> src;

	TextureGL();
	explicit TextureGL(Image& image, int blendFunc = gl::GL_SRC_ALPHA);

	void draw();
	void drawSubImage(const Rectangle<float>& pos);
	void create() override;
	void bind() override;
	void clear() override;
};

