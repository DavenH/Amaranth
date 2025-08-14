#include "TrilinearVertex.h"

void Vertex::addOwner(TrilinearCube* cube) {
    if(cube == nullptr || owners.contains(cube)) {
        return;
    }

    owners.addIfNotAlreadyThere(cube);
}
