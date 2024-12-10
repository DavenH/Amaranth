#pragma once
#include <vector>
#include "../Obj/Ref.h"
#include "../Curve/Vertex.h"

using std::vector;

class Interactor;
class Mesh;

class VertexTransformUndo {
public:
    explicit VertexTransformUndo(Interactor* itr);

    void start();
    void commitIfPending();

private:
    bool pending;

    vector<Vertex> beforeVerts;
    Ref<Interactor> interactor;
    Ref<Mesh> mesh;
};
