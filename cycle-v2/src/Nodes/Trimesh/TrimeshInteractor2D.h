#pragma once

#include "TrimeshMeshEditEvent.h"

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
    bool doCreateVertex() override;
    void reduceDetail() override {}
    void restoreDetail() override {}
    void doGlobalUIUpdate(bool) override { performUpdate(Update); }
    void setExtraElements(float x) override;
    bool isCurrentVertexHit(Point<int> mousePosition) override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;
    void deleteSelected();

    void setMeshEditedCallback(std::function<void(TrimeshMeshEditEvent)> callback);

private:
    std::function<void(TrimeshMeshEditEvent)> meshEditedCallback;
    Mesh* mesh {};
};

}
