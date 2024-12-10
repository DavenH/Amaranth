#include "Interactor.h"
#include "UndoableActions.h"
#include "VertexTransformUndo.h"
#include "../App/EditWatcher.h"
#include "../Curve/Mesh.h"
#include "../App/SingletonRepo.h"


VertexTransformUndo::VertexTransformUndo(Interactor* itr) :
		interactor(itr)
	,	pending(false) {
}


void VertexTransformUndo::start() {
	Mesh* itrMesh = interactor->getMesh();
	if(itrMesh == nullptr)
		return;

	mesh = itrMesh;
	beforeVerts.clear();

	foreach(VertIter, it, mesh->getVerts())
		beforeVerts.push_back(**it);

	pending = true;
}


void VertexTransformUndo::commitIfPending() {
	if(! pending)
        return;

    if (mesh->getNumVerts() != beforeVerts.size()) {
		jassertfalse;
		return;
	}

	vector<Vertex> afterVerts;

	bool areIdentical = true;
	foreach(VertIter, it, mesh->getVerts()) {
		Vertex* vert = *it;
		afterVerts.push_back(*vert);

		int index = it - mesh->getVertStart();

		if(areIdentical && ! (beforeVerts[index] == *vert))
			areIdentical = false;
	}

    if (!areIdentical) {
		SingletonRepo* repo = interactor->getSingletonRepo();
		TransformVerticesAction* action = new TransformVerticesAction(
				repo, interactor->getUpdateSource(),
				&mesh->getVerts(), beforeVerts, afterVerts);

		getObj(EditWatcher).addAction(action);
	}

	beforeVerts.clear();

	pending = false;
}
