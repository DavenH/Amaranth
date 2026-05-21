#pragma once

#include <Inter/Interactor2D.h>

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

private:
    Mesh* mesh {};
};

}
