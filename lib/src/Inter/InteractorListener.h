#pragma once

#include "PanelState.h"

class Interactor;
class VertCube;
class Mesh;

class InteractorListener {
public:
	virtual ~InteractorListener() = default;

	virtual void selectionChanged(Mesh* mesh, const vector<VertexFrame>& frames) {}
	virtual void focusGained(Interactor*) 	 			{}
	virtual void cubesRemoved(const vector<VertCube*>&)	{}
	virtual void cubesAdded(const vector<VertCube*>&)	{}
};

