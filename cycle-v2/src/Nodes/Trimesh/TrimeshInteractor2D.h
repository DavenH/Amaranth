#pragma once

#include <Inter/Interactor2D.h>

#include <functional>

class Mesh;

namespace CycleV2 {

class TrimeshInteractor2D : public Interactor2D {
public:
    TrimeshInteractor2D(SingletonRepo* repo, const String& name, const Dimensions& dimensions);

    void setMesh(Mesh* meshToEdit) { mesh = meshToEdit; }
    Mesh* getMesh() override { return mesh; }
    bool doesMeshChangeWarrantGlobalUpdate() override { return false; }
    void reduceDetail() override {}
    void restoreDetail() override {}
    void doGlobalUIUpdate(bool) override { performUpdate(Update); }
    void mouseUp(const MouseEvent& event) override;

    void setMeshEditedCallback(std::function<void()> callback);

private:
    std::function<void()> meshEditedCallback;
    Mesh* mesh {};
};

}
