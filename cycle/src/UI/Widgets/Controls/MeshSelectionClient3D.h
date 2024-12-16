#pragma once

#include <App/SingletonAccessor.h>
#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/AudioSourceProcessor.h>
#include <Inter/Interactor.h>
#include <Obj/Ref.h>
#include "JuceHeader.h"
#include <Definitions.h>
#include <Design/Updating/Updater.h>

#include "MeshSelectionClient.h"
#include "../Audio/SynthAudioSource.h"

class MeshSelectionClient3D;

class SelectionClientOwner {
public:
    virtual ~SelectionClientOwner() = default;
    virtual void meshSelectionChanged(Mesh* mesh) {}
    virtual void meshSelectionFinished() {}
    virtual void enterClientLock(bool audioThreadApplicable) = 0;
    virtual void exitClientLock(bool audioThreadApplicable) = 0;

    //	void setClient(MeshSelectionClient3D* client) 	{ selectionClient = client; }
    Ref<MeshSelectionClient3D> getSelectionClient() { return selectionClient.get(); }

protected:
    std::unique_ptr<MeshSelectionClient3D> selectionClient;
};

class MeshSelectionClient3D :
        public MeshSelectionClient<Mesh>
        , public SingletonAccessor {
private:
    Ref<EditWatcher> watcher;
    Ref<MeshLibrary> meshLib;
    Ref<SelectionClientOwner> owner;

    Interactor* interactor {};
    MeshRasterizer* rasterizer {};

    bool usedToViewVertsOnHover;
    int layerType {};

public:
    MeshSelectionClient3D(SelectionClientOwner* owner,
                          SingletonRepo* repo,
                          EditWatcher* watcher,
                          MeshLibrary* meshLib) : SingletonAccessor(repo, "MeshSelectionClient3D")
                                                  , owner(owner)
                                                  , watcher(watcher)
                                                  , meshLib(meshLib)
                                                  , usedToViewVertsOnHover(true) {
    }

    void enterClientLock() override {
    }

    void exitClientLock() override {
    }

    void initialise(Interactor* itr, MeshRasterizer* rast, int layerType) {
        interactor = itr;
        rasterizer = rast;

        this->layerType = layerType;
    }

    void doubleMesh() override {
        interactor->getMesh()->twin(0, 0);
        interactor->postUpdateMessage();
    }

    void setCurrentMesh(Mesh* mesh) override {
        owner->enterClientLock(true);
        interactor->resetState();

        Interactor* opposite = interactor->getOppositeInteractor();
        if (opposite != nullptr) {
            opposite->resetState();
        }

        //		meshLib->setCurrentMeshForLayer(layerType, mesh);
        meshLib->setCurrentMesh(layerType, mesh);
        interactor->clearSelectedAndCurrent();

        updateEverything(mesh);
        owner->meshSelectionFinished();

        getSetting(ViewVertsOnlyOnHover) = usedToViewVertsOnHover;

        watcher->setHaveEditedWithoutUndo(true);
        interactor->triggerRefreshUpdate();

        owner->exitClientLock(true);
    }

    void previewMesh(Mesh* mesh) override {
        owner->enterClientLock(false);
        getSetting(ViewVertsOnlyOnHover) = false;

        updateEverything(mesh);
        owner->exitClientLock(false);
    }

    void previewMeshEnded(Mesh* originalMesh) override {
        owner->enterClientLock(false);
        getSetting(ViewVertsOnlyOnHover) = usedToViewVertsOnHover;

        updateEverything(originalMesh);
        owner->exitClientLock(false);
    }

    void updateEverything(Mesh* mesh) {
        rasterizer->setMesh(mesh);
        rasterizer->update(Update);

        owner->meshSelectionChanged(mesh);
    }

    Mesh* getCurrentMesh() override {
        Mesh* mesh = rasterizer->getMesh();

        return mesh;
    }

    CriticalSection& getClientLock() {
        return getObj(SynthAudioSource).getLock();
    }

    void prepareForPopup() override {
        MeshLibrary::LayerGroup& group = meshLib->getGroup(layerType);
        group.selected.clear();

        usedToViewVertsOnHover = getSetting(ViewVertsOnlyOnHover) == 1;
    }

    int getLayerType() override {
        return layerType;
    }
};