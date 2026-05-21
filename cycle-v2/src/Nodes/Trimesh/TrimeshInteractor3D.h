#pragma once

#include <Inter/Interactor3D.h>

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

private:
    Mesh* mesh {};
};

}
