#include "Interactor.h"
#include "UndoableActions.h"
#include "VertexTransformUndo.h"

#include <Definitions.h>

#include "../App/EditWatcher.h"
#include "../Wireframe/Mesh.h"
#include "../App/SingletonRepo.h"

VertexTransformUndo::VertexTransformUndo(Interactor* itr) :
        interactor(itr)
    ,   pending(false) {
}

void VertexTransformUndo::start() {
    Mesh* itrMesh = interactor->getMesh();
    if(itrMesh == nullptr) {
        return;
    }

    mesh = itrMesh;
    beforeVerts.clear();

    for(auto& vert : mesh->getVerts()) {
        beforeVerts.push_back(*vert);
    }

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
    int i = 0;
    for(auto& vert : mesh->getVerts()) {
        afterVerts.push_back(*vert);

        if(areIdentical && ! (beforeVerts[i] == *vert)) {
            areIdentical = false;
        }

        ++i;
    }

    if (!areIdentical) {
        SingletonRepo* repo = interactor->getSingletonRepo();
        auto* action = new TransformVerticesAction(
                repo, interactor->getUpdateSource(),
                &mesh->getVerts(), beforeVerts, afterVerts);

        getObj(EditWatcher).addAction(action);
    }

    beforeVerts.clear();

    pending = false;
}
