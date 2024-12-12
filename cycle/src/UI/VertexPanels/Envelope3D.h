#pragma once

#include <UI/Panels/Panel3D.h>
#include <UI/Panels/OpenGLPanel3D.h>

class E3Rasterizer;

class Envelope3D :
		public Panel3D
	,	public Panel3D::DataRetriever
{
public:
	Envelope3D(SingletonRepo* repo);
	virtual ~Envelope3D();

	void init();
	void buttonClicked(Button* button);
	Component* getControlsComponent() { return nullptr; }

	Buffer<float> getColumnArray();
	const vector<Column>& getColumns();
};
