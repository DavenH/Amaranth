#pragma once

#include "Interactor.h"
#include "UndoableActions.h"
#include "../App/EditWatcher.h"
#include "../App/SingletonRepo.h"
#include "../Definitions.h"

class UndoableMeshProcess {
public:
    UndoableMeshProcess(Interactor* itr, const String& name) :
            interactor(itr)
        ,   mesh(itr->getMesh()) {
        Mesh* mesh = itr->getMesh();
        if (mesh == nullptr) {
            return;
        }

        // these need to be copies because the vectors
        // will change, so no using references
        beforeCubes = mesh->getCubes();
        beforeVerts = mesh->getVerts();

        SingletonRepo* repo = interactor->getSingletonRepo();

        getObj(EditWatcher).beginNewTransaction(name);
    }

    ~UndoableMeshProcess() {
        // these will be copied in the UpdateVertexVectorAction ctor,
        // but no need to copy them here
        vector<VertCube*>& afterLines = mesh->getCubes();
        vector<Vertex*>& afterVerts = mesh->getVerts();

        bool haveLines = interactor->dims.numHidden() > 0;

        // if lines are unchanged, do update with first action, otherwise skip to avoid duplicating updates
        // OR, if we have no lines, obviously do it
        const bool doUpdateWithVertexAction = beforeCubes.size() == afterLines.size() || ! haveLines;

        SingletonRepo* repo = interactor->getSingletonRepo();
        getObj(EditWatcher).addAction(
                new UpdateVertexVectorAction(interactor, &mesh->getVerts(), beforeVerts, afterVerts, doUpdateWithVertexAction), false);

        if (haveLines) {
            getObj(EditWatcher).addAction(
                    new UpdateCubeVectorAction(interactor, &mesh->getCubes(), beforeCubes, afterLines), false);
        }
    }

private:
    vector<VertCube*> beforeCubes;
    vector<Vertex*> beforeVerts;

    Ref<Interactor> interactor;
    Ref<Mesh> mesh;
};


class UndoableVertexProcess {
public:
    UndoableVertexProcess(Interactor* itr) :
            interactor(itr)
        ,   mesh(itr->getMesh()) {
        Mesh* mesh = itr->getMesh();
        if (mesh == nullptr) {
            return;
        }

        beforeVerts = mesh->getVerts();
    }

    ~UndoableVertexProcess() {
        SingletonRepo* repo = interactor->getSingletonRepo();

        getObj(EditWatcher).addAction(
                new UpdateVertexVectorAction(interactor, &mesh->getVerts(), beforeVerts, mesh->getVerts(), true));
    }

private:
    vector<Vertex*> beforeVerts;

    Ref<Interactor> interactor;
    Ref<Mesh> mesh;
};
