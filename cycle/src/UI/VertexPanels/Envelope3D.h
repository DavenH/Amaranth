#pragma once

#include <UI/Panels/Panel3D.h>
#include <UI/Panels/OpenGLPanel3D.h>

class E3Rasterizer;

class Envelope3D :
		public Panel3D
	,	public Panel3D::DataRetriever
{
public:
	explicit Envelope3D(SingletonRepo* repo);
	~Envelope3D() override;

	void init() override;
	void buttonClicked(Button* button);
	static Component* getControlsComponent() { return nullptr; }

	Buffer<float> getColumnArray() override;
	const vector<Column>& getColumns() override;
};
