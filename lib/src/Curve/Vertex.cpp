#include "Vertex.h"

void Vertex::addOwner(VertCube* cube) {
    if (cube == nullptr || owners.contains(cube)) {
        return;
    }

    owners.addIfNotAlreadyThere(cube);
}
