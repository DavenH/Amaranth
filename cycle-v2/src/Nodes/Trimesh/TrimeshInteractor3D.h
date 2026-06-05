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
    bool locateClosestElement() override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;

    void setPrimaryViewAxis(int axis);
    void setMeshEditedCallback(std::function<void(bool)> callback);

private:
    std::function<void(bool)> meshEditedCallback;
    Mesh* mesh {};
};

}
