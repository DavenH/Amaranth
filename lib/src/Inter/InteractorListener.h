#ifndef INTER_INTERACTORLISTENER_H_
#define INTER_INTERACTORLISTENER_H_

#include "PanelState.h"

class Interactor;
class VertCube;

class InteractorListener {
public:
	virtual void selectionChanged(Mesh* mesh, const vector<VertexFrame>& frames) {}
	virtual void focusGained(Interactor*) 	 			{}
	virtual void cubesRemoved(const vector<VertCube*>&)	{}
	virtual void cubesAdded(const vector<VertCube*>&)	{}
};

#endif
