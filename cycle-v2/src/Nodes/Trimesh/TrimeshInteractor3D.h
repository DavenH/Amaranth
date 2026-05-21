#pragma once

#include <Inter/Interactor3D.h>

#include <functional>

class Mesh;

namespace CycleV2 {

class TrimeshInteractor3D : public Interactor3D {
public:
    TrimeshInteractor3D(SingletonRepo* repo, const String& name);

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
