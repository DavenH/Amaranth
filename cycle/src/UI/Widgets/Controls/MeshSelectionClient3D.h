#pragma once

#include <App/MeshLibrary.h>
#include <App/SingletonAccessor.h>
#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/AudioSourceProcessor.h>
#include <Curve/Rasterization/Interfaces/MeshBindableRasterizer.h>
#include <Curve/Rasterization/Interfaces/RasterizerUpdateTarget.h>
#include <Inter/Interactor.h>
#include <Obj/Ref.h>
#include "JuceHeader.h"
#include <Definitions.h>

#include "MeshSelectionClient.h"
#include "../../../Audio/SynthAudioSource.h"

class MeshSelectionClient3D;

class SelectionClientOwner {
public:
    virtual ~SelectionClientOwner() = default;
    virtual void meshSelectionChanged(Mesh* mesh) {}
    virtual void meshSelectionFinished() {}
    virtual void enterClientLock(bool audioThreadApplicable) = 0;
    virtual void exitClientLock(bool audioThreadApplicable) = 0;

    [[nodiscard]] Ref<MeshSelectionClient3D> getSelectionClient() const { return selectionClient.get(); }

protected:
    std::unique_ptr<MeshSelectionClient3D> selectionClient;
};

class MeshSelectionClient3D :
        public MeshSelectionClient<Mesh>
        , public SingletonAccessor
        , public MeshLibrary::Listener {
private:
    Ref<EditWatcher> watcher;
    Ref<MeshLibrary> meshLib;
    Ref<SelectionClientOwner> owner;

    Interactor* interactor {};
    Rasterization::MeshBindableRasterizer* rasterizer {};
    Rasterization::RasterizerUpdateTarget* updateTarget {};

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
        meshLib->addListener(this);
    }

    void enterClientLock() override {
    }

    void exitClientLock() override {
    }

    void initialise(Interactor* itr, MeshRasterizer* rast, int layerType) {
        initialise(itr, rast, rast, layerType);
    }

    void initialise(
            Interactor* itr,
            Rasterization::MeshBindableRasterizer* rast,
            Rasterization::RasterizerUpdateTarget* target,
            int layerType) {
        interactor = itr;
        rasterizer = rast;
        updateTarget = target;

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

        interactor->clearSelectedAndCurrent();
        meshLib->setCurrentMesh(layerType, mesh);

        getSetting(ViewVertsOnlyOnHover) = usedToViewVertsOnHover;
        watcher->setHaveEditedWithoutUndo(true);
        owner->meshSelectionFinished();
        interactor->triggerRefreshUpdate();
        owner->exitClientLock(true);
    }

    void previewMesh(Mesh* mesh) override {
        owner->enterClientLock(false);
        getSetting(ViewVertsOnlyOnHover) = false;
        meshLib->beginPreviewMesh(layerType, mesh);
        owner->exitClientLock(false);
    }

    void previewMeshEnded(Mesh* originalMesh) override {
        ignoreUnused(originalMesh);
        owner->enterClientLock(false);
        getSetting(ViewVertsOnlyOnHover) = usedToViewVertsOnHover;
        meshLib->endPreviewMesh(layerType);
        owner->exitClientLock(false);
    }

    void effectiveMeshChanged(int groupId, Mesh* mesh) override {
        if (groupId != layerType) {
            return;
        }

        updateEverything(mesh);
    }

    void updateEverything(Mesh* mesh) {
        rasterizer->setMesh(mesh);
        updateTarget->updateRasterizer(Update);

        owner->meshSelectionChanged(mesh);
    }

    Mesh* getCurrentMesh() override {
        return meshLib->getEffectiveMesh(layerType);
    }

    CriticalSection& getClientLock() {
        return getObj(SynthAudioSource).getLock();
    }

    void prepareForPopup() override {
        MeshLibrary::LayerGroup& group = meshLib->getLayerGroup(layerType);
        group.selected.clear();

        usedToViewVertsOnHover = getSetting(ViewVertsOnlyOnHover) == 1;
    }

    int getLayerType() override {
        return layerType;
    }
};
