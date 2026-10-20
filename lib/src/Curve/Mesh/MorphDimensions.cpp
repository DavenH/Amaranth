#include "MorphDimensions.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h>

#include <map>
#include <vector>

int MorphDimensions::cornerBit(int dimension) {
    return dimension == Vertex::Time ? 4 : dimension == Vertex::Red ? 2 : dimension == Vertex::Blue ? 1 : 0;
}

bool MorphDimensions::active(int dimension) const {
    return (cornerMask() & cornerBit(dimension)) != 0;
}

int MorphDimensions::cornerMask() const {
    return count == 3 ? 7 : count == 2 ? (plane == Plane::YellowRed ? 6 : 3) : count == 1 ? 4 : 0;
}

MorphPosition MorphDimensions::constrain(
        const MorphPosition& position,
        const MorphPosition& fallback) const {
    MorphPosition result = position;
    for (const int axis : axes) {
        if (!active(axis)) {
            result[axis].setValueDirect(fallback[axis]);
        }
    }
    return result;
}

void MorphDimensions::project(
        Mesh& mesh,
        MorphDimensions retained,
        const MorphPosition& position,
        GuideCurveProvider* guides,
        std::vector<int>* retainedCubeIndices) {
    using Corners = std::array<Vertex, 8>;
    std::vector<Corners> projected;
    projected.reserve(mesh.getCubes().size());
    std::vector<VertCube*> retainedCubes;
    if (retainedCubeIndices != nullptr) {
        retainedCubeIndices->clear();
    }
    int originalIndex = -1;
    for (auto* cube : mesh.getCubes()) {
        ++originalIndex;
        bool overlaps = true;
        if (retained.axisCount() <= 1) {
            VertCube::ReductionData section;
            Rasterization::TrilinearMeshSlicer().slice(*cube, Vertex::Time, section, position);
            overlaps = retained.axisCount() == 0 ? section.pointOverlaps : section.lineOverlaps;
        } else if (retained.axisCount() == 2) {
            const int dismissed = retained.active(Vertex::Time) ? Vertex::Blue : Vertex::Time;
            float minimum = 1.f;
            float maximum = 0.f;
            for (int corner = 0; corner < 8; ++corner) {
                const float coordinate = cube->getVertex(corner)->values[dismissed];
                minimum = jmin(minimum, coordinate);
                maximum = jmax(maximum, coordinate);
            }
            const float coordinate = position[dismissed];
            overlaps = coordinate >= minimum
                    && (coordinate < maximum || ((coordinate == 0.f || coordinate == 1.f) && coordinate == maximum));
        }
        if (!overlaps) {
            if (auto* envelope = dynamic_cast<EnvelopeMesh*>(&mesh)) {
                envelope->sustainCubes.erase(cube);
                envelope->loopCubes.erase(cube);
            }
            delete cube;
            continue;
        }
        if (retainedCubeIndices != nullptr) {
            retainedCubeIndices->push_back(originalIndex);
        }
        retainedCubes.push_back(cube);
        Corners corners;
        for (int corner = 0; corner < 8; ++corner) {
            corners[(size_t) corner] = *cube->getVertex(corner);
        }
        // Match mature trilinear reduction order: Blue, Red, then Yellow.
        for (int index = 2; index >= 0; --index) {
            const int axis = axes[(size_t) index];
            const int bit = poleBits[(size_t) index];
            if (retained.active(axis)) {
                continue;
            }
            for (int corner = 0; corner < 8; ++corner) {
                if ((corner & bit) != 0) {
                    continue;
                }
                Vertex reduced;
                VertCube::vertexAt(position[axis], axis,
                        &corners[(size_t) corner], &corners[(size_t) (corner | bit)], &reduced);
                corners[(size_t) corner] = reduced;
                corners[(size_t) (corner | bit)] = reduced;
                corners[(size_t) corner].values[axis] = 0.f;
                corners[(size_t) (corner | bit)].values[axis] = 1.f;
            }
        }
        if (guides != nullptr) {
            for (auto& corner : corners) {
                MorphPosition evaluation = position;
                for (const int axis : axes) {
                    if (retained.active(axis)) {
                        evaluation[axis].setValueDirect(corner.values[axis]);
                    }
                }
                for (const int axis : { Vertex::Red, Vertex::Blue }) {
                    if (retained.active(axis)) {
                        continue;
                    }
                    corner.values[Vertex::Phase] += Rasterization::GuideCurvePolicy::morphAxisOffset(
                            *cube, evaluation, axis, [&](int guide, float progress) {
                                return guides->getTableValue(guide, progress, {});
                            });
                }
            }
        }
        projected.push_back(std::move(corners));
    }

    mesh.getCubes() = std::move(retainedCubes);

    // Reuse equal projected corners to preserve adjacency, splitting former shared
    // poles when their dismissed coordinate must now become opposite unit poles.
    std::map<std::array<float, Vertex::numElements>, Vertex*> vertices;
    for (auto* vertex : mesh.getVerts()) {
        delete vertex;
    }
    mesh.getVerts().clear();
    for (size_t cell = 0; cell < projected.size(); ++cell) {
        auto* cube = mesh.getCubes()[cell];
        for (int corner = 0; corner < 8; ++corner) {
            std::array<float, Vertex::numElements> values;
            std::copy_n(projected[cell][(size_t) corner].values, values.size(), values.begin());
            auto& vertex = vertices[values];
            if (vertex == nullptr) {
                vertex = new Vertex();
                std::copy(values.begin(), values.end(), vertex->values);
                mesh.addVertex(vertex);
            }
            vertex->addOwner(cube);
            cube->lineVerts[corner] = vertex;
        }
    }
}
