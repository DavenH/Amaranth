#include "TrilinearCube.h"
#include "MorphPosition.h"
#include "../../Mesh.h"
#include "../../Vertex/Vertex2.h"
#include "../../../Algo/Resampling.h"
#include "../../../Util/Arithmetic.h"
#include "../../../Util/CommonEnums.h"
#include "../../../Util/Geometry.h"
#include "../../../Util/NumberUtils.h"

TrilinearCube::TrilinearCube() {
    init();
}

TrilinearCube::TrilinearCube(Mesh* mesh) {
    init();

    for (auto & vert : lineVerts) {
        vert = new Vertex();
        vert->addOwner(this);
        mesh->addVertex(vert);
    }

    validate();
}

TrilinearCube::TrilinearCube(TrilinearCube& cube) {
    setPropertiesFrom(&cube);
    validate();
}

TrilinearCube::~TrilinearCube() {
    init();

    for(auto& lineVert : lineVerts) {
        lineVert = nullptr;
    }
}

void TrilinearCube::init() {
    float defaultComp = 0.5f + NumberUtils::toDecibels(1.5) / 60.f;

    for(float& pathGain : pathGains) {
        pathGain = 0.5;
    }

    pathGains[Vertex::Time] = defaultComp;

    resetProperties();
}

void TrilinearCube::getPoles(Vertex const* vert, bool& time, bool& red, bool& blue) const {
    getPoles(indexOf(vert), time, red, blue);
}

void TrilinearCube::getPoles(int index, bool& time, bool& red, bool& blue) {
    time = index / 4        > 0;
    red  = (index / 2) % 2  > 0;
    blue = index % 2        > 0;
}

TrilinearCube::Face TrilinearCube::getFace(int primeDim, bool pole) const {
    Face face(primeDim);

    switch (primeDim) {
        case Vertex::Time:
            if (pole == LowPole) {
                face.v00 = lineVerts[y0r0b0];
                face.v01 = lineVerts[y0r0b1];
                face.v10 = lineVerts[y0r1b0];
                face.v11 = lineVerts[y0r1b1];
            } else if (pole == HighPole) {
                face.v00 = lineVerts[y1r0b0];
                face.v01 = lineVerts[y1r0b1];
                face.v10 = lineVerts[y1r1b0];
                face.v11 = lineVerts[y1r1b1];
            }
            break;

        case Vertex::Red:
            if (pole == LowPole) {
                face.v00 = lineVerts[y0r0b0];
                face.v01 = lineVerts[y0r0b1];
                face.v10 = lineVerts[y1r0b0];
                face.v11 = lineVerts[y1r0b1];
            } else if (pole == HighPole) {
                face.v00 = lineVerts[y0r1b0];
                face.v01 = lineVerts[y0r1b1];
                face.v10 = lineVerts[y1r1b0];
                face.v11 = lineVerts[y1r1b1];
            }
            break;

        case Vertex::Blue:
            if (pole == LowPole) {
                face.v00 = lineVerts[y0r0b0];
                face.v01 = lineVerts[y0r1b0];
                face.v10 = lineVerts[y1r0b0];
                face.v11 = lineVerts[y1r1b0];
            } else if (pole == HighPole) {
                face.v00 = lineVerts[y0r0b1];
                face.v01 = lineVerts[y0r1b1];
                face.v10 = lineVerts[y1r0b1];
                face.v11 = lineVerts[y1r1b1];
            }
            break;
        default: throw std::invalid_argument("TrilinearCube::getFace(): invalid face type");
    }

    return face;
}

void TrilinearCube::setFace(const Face& face, int primeDim, bool polarity) {
    switch (primeDim) {
        case Vertex::Time:
            if (polarity == LowPole) {
                lineVerts[y0r0b0] = face.v00;
                lineVerts[y0r0b1] = face.v01;
                lineVerts[y0r1b0] = face.v10;
                lineVerts[y0r1b1] = face.v11;
            } else if (polarity == HighPole) {
                lineVerts[y1r0b0] = face.v00;
                lineVerts[y1r0b1] = face.v01;
                lineVerts[y1r1b0] = face.v10;
                lineVerts[y1r1b1] = face.v11;
            }

            break;

        case Vertex::Red:
            if (polarity == LowPole) {
                lineVerts[y0r0b0] = face.v00;
                lineVerts[y0r0b1] = face.v01;
                lineVerts[y1r0b0] = face.v10;
                lineVerts[y1r0b1] = face.v11;
            } else if (polarity == HighPole) {
                lineVerts[y0r1b0] = face.v00;
                lineVerts[y0r1b1] = face.v01;
                lineVerts[y1r1b0] = face.v10;
                lineVerts[y1r1b1] = face.v11;
            }

            break;

        case Vertex::Blue:
            if (polarity == LowPole) {
                lineVerts[y0r0b0] = face.v00;
                lineVerts[y0r1b0] = face.v01;
                lineVerts[y1r0b0] = face.v10;
                lineVerts[y1r1b0] = face.v11;
            } else if (polarity == HighPole) {
                lineVerts[y0r0b1] = face.v00;
                lineVerts[y0r1b1] = face.v01;
                lineVerts[y1r0b1] = face.v10;
                lineVerts[y1r1b1] = face.v11;
            }

            break;
        default: throw std::invalid_argument("TrilinearCube::getFace(): invalid face type");

    }

    face.v00->addOwner(this);
    face.v01->addOwner(this);
    face.v10->addOwner(this);
    face.v11->addOwner(this);
}

void TrilinearCube::getFinalIntercept(ReductionData& data, const MorphPosition& pos) const {
    // we're interpolating all dimensions here so it's arbitrary which one we use; just stay consistent
    const int arbitraryDimToUse = Vertex::Time;

    getInterceptsFast(arbitraryDimToUse, data, pos);

    if(! data.pointOverlaps) {
        return;
    }

    vertexAt(pos.time, arbitraryDimToUse, &data.v0, &data.v1, &data.v);
}

void TrilinearCube::getInterceptsAccurate(int dim, ReductionData& data, const MorphPosition& pos) const {
    data.pointOverlaps = true;
    data.lineOverlaps = true;

    Vertex2 poleA, poleB;
    int dimX = 0, dimY = 0;

    const bool poles[] = { LowPole, HighPole };

    MorphPosition::getOtherDims(dim, dimX, dimY);
    Vertex2 point(pos[dimX], pos[dimY]);
    float primeVal = pos[dim];

    for(int i = 0; i < 2; ++i) {
        Face face = getFace(dim, poles[i]);

        float xFracA = 1, xFracB = 1;
        float yFracA = 1, yFracB = 1;

        Vertex2 nnv(face.v00->values[dimX], face.v00->values[dimY]);
        Vertex2 nxv(face.v01->values[dimX], face.v01->values[dimY]);
        Vertex2 xnv(face.v10->values[dimX], face.v10->values[dimY]);
        Vertex2 xxv(face.v11->values[dimX], face.v11->values[dimY]);

        Vertex2 diffYminX = nxv - nnv;
        Vertex2 diffXminY = xnv - nnv;
        Vertex2 diffYmaxX = xxv - xnv;
        Vertex2 diffXmaxY = xxv - nxv;

        int intersectCount = 0;

        float denom = (nnv - nxv).cross(xnv - xxv);
        if (fabsf(denom) > 1e-4f) {
            Vertex2 icptY = (nnv - nxv) * ((xxv - nxv).cross(xnv - xxv) / denom) + nxv;

            if(Geometry::doLineSegmentsIntersect(icptY.x, icptY.y, point.x, point.y, nnv.x, nnv.y, nxv.x, nxv.y))
                ++intersectCount;

            float icptDenom = (point - icptY).cross(nnv - xnv);
            xFracA = (xnv - icptY).cross(nnv - xnv) / icptDenom;

            icptDenom = (point - icptY).cross(nxv - xxv);
            xFracB = (xxv - icptY).cross(nxv - xxv) / icptDenom;
        } else {
            xFracA = xFracB = (point.x - nnv.x) / diffXminY.x;
        }

        if (xFracA >= 1 || xFracA < 0) {
            data.pointOverlaps = false;
            data.lineOverlaps = false;
        }

        denom = (nxv - xxv).cross(nnv - xnv);
        if (fabsf(denom) > 1e-4f) {
            Vertex2 icptX   = (nxv - xxv) * ((nxv - xxv).cross(nnv - xnv) / denom) + xxv;
            Vertex2 diffPI  = point - icptX;

            float icptDenom = (point - icptX).cross(nxv - nnv);
            yFracA          = (nnv - icptX).cross(nxv - nnv) / icptDenom;

            icptDenom       = (point - icptX).cross(xxv - xnv);
            yFracB          = (xnv - icptX).cross(xxv - xnv) / icptDenom;
        } else {
            yFracA = yFracB = (point.y - nnv.y) / diffYminX.y;
        }

        if (xFracA >= 1 || xFracA < 0) {
            data.pointOverlaps = false;
            data.lineOverlaps = false;
        }

        float nnFrac = (1 - xFracA) * (1 - yFracA);
        float xnFrac = xFracA       * (1 - yFracA);
        float nxFrac = (1 - xFracB) * yFracB;
        float xxFrac = xFracB       * yFracB;

        data[i] = *face.v00 * nnFrac
                  + *face.v10 * xnFrac
                  + *face.v01 * nxFrac
                  + *face.v11 * xxFrac;
    }

    if (data.pointOverlaps) {
        float valA = data.v0.values[dim];
        float valB = data.v1.values[dim];

        // we want the containment test to include the lesser pointer and exclude the greater point
        data.pointOverlaps &= (valA > valB) ?
                valB <= primeVal && primeVal < valA :
                valA <= primeVal && primeVal < valB;
    }
}

bool TrilinearCube::intersectsMorphRect(int dim, ReductionData& data, const MorphPosition& pos) const {
    Rectangle<float> rect = pos.toRect(dim);

    Face midFace(data, dim);
    bool overlapsDim = midFace.merge(this, pos[dim]);

    if(overlapsDim) {
        int dimX, dimY;
        MorphPosition::getOtherDims(dim, dimX, dimY);

        Rectangle r1(Point(midFace.v00->values[dimX], midFace.v00->values[dimY]),
                     Point(midFace.v11->values[dimX], midFace.v11->values[dimY]));

        Rectangle r2(Point(midFace.v01->values[dimX], midFace.v01->values[dimY]),
                     Point(midFace.v10->values[dimX], midFace.v10->values[dimY]));

        return rect.intersects(r1.getUnion(r2));
    }

    return false;
}

void TrilinearCube::getInterceptsFast(int dim, ReductionData& data, const MorphPosition& pos) const {
    data.pointOverlaps = false;
    data.lineOverlaps = false;

    int dimX = Vertex::Red, dimY = Vertex::Blue;

    MorphPosition::getOtherDims(dim, dimX, dimY);
    Vertex2 point(pos[dimX], pos[dimY]);

    Face lowFace = getFace(dim, LowPole);

    float nx = lowFace.v00->values[dimX];
    float ny = lowFace.v00->values[dimY];
    float xx = lowFace.v11->values[dimX];
    float xy = lowFace.v11->values[dimY];

    float x1 = jmin(nx, xx);
    float y1 = jmin(ny, xy);
    float x2 = jmax(nx, xx);
    float y2 = jmax(ny, xy);

    // only want upper inclusivity at the unit boundaries
    if(x2 == 1.f) { x2 += 0.000001f; }
    if(y2 == 1.f) { y2 += 0.000001f; }

    if (point.x >= x1 && point.x < x2 && point.y >= y1 && point.y < y2) {
        vertexAt(point.y, dimY, lowFace.v00, lowFace.v01, &data.v00);
        vertexAt(point.y, dimY, lowFace.v10, lowFace.v11, &data.v10);
        vertexAt(point.x, dimX, &data.v00,   &data.v10,   &data.v0);

        Face highFace = getFace(dim, HighPole);

        float nnx = highFace.v00->values[dimX];
        float nny = highFace.v00->values[dimY];
        float xxx = highFace.v11->values[dimX];
        float xxy = highFace.v11->values[dimY];

        x1 = jmin(nnx, xxx);
        y1 = jmin(nny, xxy);
        x2 = jmax(nnx, xxx);
        y2 = jmax(nny, xxy);

        if(x2 == 1.f) { x2 += 0.000001f; }
        if(y2 == 1.f) { y2 += 0.000001f; }

        if (point.x >= x1 && point.x < x2 && point.y >= y1 && point.y < y2) {
            vertexAt(point.y, dimY, highFace.v00, highFace.v01, &data.v01);
            vertexAt(point.y, dimY, highFace.v10, highFace.v11, &data.v11);
            vertexAt(point.x, dimX, &data.v01,    &data.v11,    &data.v1);

            data.lineOverlaps = true;

            float primeVal = pos[dim];
            float a = data.v0.values[dim];
            float b = data.v1.values[dim];

            data.pointOverlaps = (primeVal == 1.f || primeVal == 0.f) ?
                    a == primeVal || b == primeVal : NumberUtils::withinExclUpper(primeVal, jmin(a, b), jmax(a, b));
        }
    }
}

void TrilinearCube::getMultidimIntercept(
        Vertex* a1, Vertex* a2,
        Vertex* b1, Vertex* b2,
        Vertex* v1, Vertex* v2,
        float& redLow, float& redHigh,
        float& blueLow, float& blueHigh,
        const MorphPosition& pos) const {
    // check if contained
    Vertex k1, k2, k3, k4;

    vertexAt(pos.blue, Vertex::Blue, lineVerts[y0r0b0], lineVerts[y0r0b1], a1);
    vertexAt(pos.blue, Vertex::Blue, lineVerts[y0r1b0], lineVerts[y0r1b1], b1);
    vertexAt(pos.red,  Vertex::Red, a1, b1, v1);

    vertexAt(pos.red,  Vertex::Red, lineVerts[y0r0b0], lineVerts[y0r1b0], &k1);
    vertexAt(pos.red,  Vertex::Red, lineVerts[y0r0b1], lineVerts[y0r1b1], &k2);

    vertexAt(pos.blue, Vertex::Blue, lineVerts[y1r0b0], lineVerts[y1r0b1], a2);
    vertexAt(pos.blue, Vertex::Blue, lineVerts[y1r1b0], lineVerts[y1r1b1], b2);
    vertexAt(pos.red,  Vertex::Red, a2, b2, v2);

    vertexAt(pos.red,  Vertex::Red, lineVerts[y1r0b0], lineVerts[y1r1b0], &k3);
    vertexAt(pos.red,  Vertex::Red, lineVerts[y1r0b1], lineVerts[y1r1b1], &k4);

    redLow  = Resampling::lerp(k1.values[Vertex::Time],  k1.values[Vertex::Phase],
                               k3.values[Vertex::Time],  k3.values[Vertex::Phase], pos.time);

    redHigh = Resampling::lerp(k2.values[Vertex::Time],  k2.values[Vertex::Phase],
                               k4.values[Vertex::Time],  k4.values[Vertex::Phase], pos.time);

    blueLow = Resampling::lerp(a1->values[Vertex::Time], a1->values[Vertex::Phase],
                               a2->values[Vertex::Time], a2->values[Vertex::Phase], pos.time);

    blueHigh= Resampling::lerp(b1->values[Vertex::Time], b1->values[Vertex::Phase],
                               b2->values[Vertex::Time], b2->values[Vertex::Phase], pos.time);
}

bool TrilinearCube::hasCommonVertsWith(const TrilinearCube* cube) const {
    bool similarVerts = false;

    for(int i = 0; i < numVerts / 2; ++i) {
        similarVerts |= lineVerts[i / 2]     == cube->lineVerts[i / 2];
        similarVerts |= lineVerts[i / 2 + 1] == cube->lineVerts[i / 2];
        similarVerts |= lineVerts[i / 2]     == cube->lineVerts[i / 2 + 1];
        similarVerts |= lineVerts[i / 2 + 1] == cube->lineVerts[i / 2 + 1];
    }

    return similarVerts;
}

int TrilinearCube::indexOf(Vertex const* vertex) const {
    for (int i = 0; i < numVerts; ++i) {
        if (vertex == lineVerts[i]) {
            return i;
        }
    }

    return CommonEnums::Null;
}

Vertex* TrilinearCube::getVertex(int index) const {
    jassert(index < numVerts);

    return lineVerts[index];
}

Array<Vertex*> TrilinearCube::toArray() const {
    return {lineVerts, numVerts};
}

bool TrilinearCube::setVertex(Vertex* vertex, int index) {
    jassert(index < numVerts);

    for (int i = 0; i < numVerts; ++i) {
        if (vertex == lineVerts[i] && i != index) {
            lineVerts[index] = nullptr;
            return false;
        }
    }

    lineVerts[index] = vertex;

    return true;
}

TrilinearCube::Face TrilinearCube::getFace(int dim, Vertex const* vertex) const {
    bool polarity = poleOf(dim, vertex);

    return getFace(dim, polarity);
}

Vertex* TrilinearCube::getVertWithPolarities(bool time, bool red, bool blue) const {
    return lineVerts[int(time) * 4 + int(red) * 2 + int(blue) * 1];
}

Vertex* TrilinearCube::getOtherVertexAlong(int dim, Vertex const* vertex) const {
    bool timePole = poleOf(Vertex::Time, vertex);
    bool redPole  = poleOf(Vertex::Red,  vertex);
    bool bluePole = poleOf(Vertex::Blue, vertex);

    return getVertWithPolarities(dim == Vertex::Time ? ! timePole : timePole,
                                 dim == Vertex::Red  ? ! redPole  : redPole,
                                 dim == Vertex::Blue ? ! bluePole : bluePole);
}

Vertex* TrilinearCube::findClosestVertex(const MorphPosition& pos) const {
    float minDist = 1000, dist;
    Vertex* closest;

    for (auto lineVert : lineVerts) {
        dist = 0;

        dist += fabsf(pos.time - lineVert->values[Vertex::Time]);
        dist += fabsf(pos.red  - lineVert->values[Vertex::Red] );
        dist += fabsf(pos.blue - lineVert->values[Vertex::Blue]);

        if (dist < minDist) {
            minDist = dist;
            closest = lineVert;
        }
    }

    return closest;
}

void TrilinearCube::initVerts(const MorphPosition& pos) {
    for (int i = 0; i < numVerts; ++i) {
        bool time, red, blue;
        getPoles(i, time, red, blue);

        Vertex* vert = lineVerts[i];
        vert->values[Vertex::Time] = time ? pos.timeEnd() : pos.time;
        vert->values[Vertex::Red]  = red  ? pos.redEnd()  : pos.red;
        vert->values[Vertex::Blue] = blue ? pos.blueEnd() : pos.blue;
    }
}

bool TrilinearCube::poleOf(int dim, Vertex const* poleVert) const {
    bool polarity = LowPole;
    bool time, red, blue;
    getPoles(poleVert, time, red, blue);

    switch (dim) {
        case Vertex::Time: return time;
        case Vertex::Red:  return red;
        case Vertex::Blue: return blue;

        default:
            jassertfalse;
            return time;
    }
}

void TrilinearCube::orphanVerts() {
    for (auto& lineVert : lineVerts)
        lineVert->removeOwner(this);
}

void TrilinearCube::setPropertiesFrom(TrilinearCube const* other) {
    for (int i = Vertex::Time; i <= Vertex::Curve; ++i) {
        pathAt(i) = other->pathAt(i);
        pathGainAt(i) = other->pathGainAt(i);
    }
}

void TrilinearCube::resetProperties() {
    for (char& pathChan : pathChans) {
        pathChan = CommonEnums::Null;
    }
}

void TrilinearCube::validate() {
    for (auto vert : lineVerts) {
        jassert(vert != nullptr);
        jassert(vert != reinterpret_cast<Vertex*>(0xcdcdcdcd));
        jassert(vert->isOwnedBy(this));
    }
}

float TrilinearCube::pathAbsGain(int dim) const {
    if (dim < 0) {
        return CommonEnums::Null;
    }

    float value = pathGainAt(dim);

    if(value == 0.5) {
        return 1;
    }

    return NumberUtils::fromDecibels(60 * (value - 0.5));
}

bool TrilinearCube::hasCommonEdgeAlong(int dim, TrilinearCube const* cube) const {
    Face lowFace  = getFace(dim, LowPole);
    Face highFace = getFace(dim, HighPole);

    for (int i = 0; i < numVerts / 2; ++i) {
        if(cube->containsEdge(Edge(lowFace[i], highFace[i]))) {
            return true;
        }
    }

    return false;
}

bool TrilinearCube::containsEdge(const Edge& edge) const {
    // fails when testing diagonal verts, but I doubt this could ever happen
    return indexOf(edge.v0) != CommonEnums::Null && indexOf(edge.v1) != CommonEnums::Null;
}

bool TrilinearCube::dimensionsAt(float x, int axis, Vertex const* one, Vertex const* two, Vertex* vertex) {
    float diff = two->values[axis] - one->values[axis];

    if (diff == 0.f) {
        *vertex = *two;
        return true;
    }

    float mult = (x - one->values[axis]) / diff;

    if(mult < 0.f || mult > 1.f) {
        return false;
    }

    NumberUtils::constrain(mult, 0.f, 1.f);

    vertex->values[Vertex::Time]    = one->values[Vertex::Time]  +  mult * (two->values[Vertex::Time]   - one->values[Vertex::Time]  );
    vertex->values[Vertex::Red]     = one->values[Vertex::Red]   +  mult * (two->values[Vertex::Red]    - one->values[Vertex::Red]   );
    vertex->values[Vertex::Blue]    = one->values[Vertex::Blue]  +  mult * (two->values[Vertex::Blue]   - one->values[Vertex::Blue]  );

    return true;
}

void TrilinearCube::positionAt(float x, int axis, Vertex const* one, Vertex const* two, Vertex* vertex) {
    float diff = two->values[axis] - one->values[axis];

    if (diff == 0.f) {
        *vertex = *two;
        return;
    }

    float mult = (x - one->values[axis]) / diff;
    NumberUtils::constrain(mult, 0.f, 1.f);

    vertex->values[Vertex::Phase] = one->values[Vertex::Phase] + mult * (two->values[Vertex::Phase] - one->values[Vertex::Phase]);
    vertex->values[Vertex::Amp]   = one->values[Vertex::Amp]   + mult * (two->values[Vertex::Amp]   - one->values[Vertex::Amp]);
}


Array<TrilinearCube*> TrilinearCube::getAllAdjacentCubes(int dim, const MorphPosition& pos) {
    ReductionData data;
    Array<TrilinearCube*> cubes;

    getAdjacentCubes(dim, cubes, data, pos);
    return cubes;
}

#include "../../../Definitions.h"

void TrilinearCube::getAdjacentCubes(int dim, Array<TrilinearCube*>& cubes, ReductionData& data, const MorphPosition& pos) {
    if (!intersectsMorphRect(dim, data, pos))
        cubes.add(this);

    int dimX, dimY;
    MorphPosition::getOtherDims(dim, dimX, dimY);

    Array<TrilinearCube*> newCubes;
    newCubes.add(getAdjacentCube(dimX, LowPole));
    newCubes.add(getAdjacentCube(dimX, HighPole));
    newCubes.add(getAdjacentCube(dimY, LowPole));
    newCubes.add(getAdjacentCube(dimY, HighPole));

    for(auto cube : newCubes) {
        if(cube != nullptr && ! cubes.contains(cube)) {
            cube->getAdjacentCubes(dim, cubes, data, pos);
        }
    }
}

TrilinearCube* TrilinearCube::getAdjacentCube(int dim, bool pole) const {
    Face face = getFace(dim, pole);

    Vertex* first = face.v00;
    Vertex* last = face.v11;

    for(auto v1 : first->owners) {
        for(auto v2 : last->owners) {
            if (v1 == v2 && v1 != this) {
                return v1;
            }
        }
    }

    return nullptr;
}

void TrilinearCube::vertexAt(float x, int axis, Vertex const* one, Vertex const* two, Vertex* vertex) {
    float diff = two->values[axis] - one->values[axis];

    if (diff == 0.f) {
        *vertex = *two;
        return;
    }

    float mult = (x - one->values[axis]) / diff;
    NumberUtils::constrain(mult, 0.f, 1.f);

    vertex->values[Vertex::Time ] = one->values[Vertex::Time ] + mult * (two->values[Vertex::Time ] - one->values[Vertex::Time ]);
    vertex->values[Vertex::Phase] = one->values[Vertex::Phase] + mult * (two->values[Vertex::Phase] - one->values[Vertex::Phase]);
    vertex->values[Vertex::Amp  ] = one->values[Vertex::Amp  ] + mult * (two->values[Vertex::Amp  ] - one->values[Vertex::Amp  ]);
    vertex->values[Vertex::Red  ] = one->values[Vertex::Red  ] + mult * (two->values[Vertex::Red  ] - one->values[Vertex::Red  ]);
    vertex->values[Vertex::Blue ] = one->values[Vertex::Blue ] + mult * (two->values[Vertex::Blue ] - one->values[Vertex::Blue ]);
    vertex->values[Vertex::Curve] = one->values[Vertex::Curve] + mult * (two->values[Vertex::Curve] - one->values[Vertex::Curve]);
}

MorphPosition TrilinearCube::getCentroid(bool isEnv) const {
    MorphPosition pos;

    int limit = isEnv ? numVerts / 2 : numVerts;
    float invLimit = 1 / float(limit);

    for (int i = 0; i < limit; ++i) {
        pos.time.setValueDirect(pos.time.getTargetValue() + lineVerts[i]->values[Vertex::Time] * invLimit);
        pos.red.setValueDirect(pos.time.getTargetValue() + lineVerts[i]->values[Vertex::Red] * invLimit);
        pos.blue.setValueDirect(pos.time.getTargetValue() + lineVerts[i]->values[Vertex::Blue] * invLimit);
    }

    return pos;
}

float TrilinearCube::getPortionAlong(int dim, const MorphPosition& morph) const {
    float valA, valB;
    morph.getOtherPos(dim, valA, valB);

    float kxx = valA * valB;
    float kxn = valA * (1 - valB);
    float knx = (1 - valA) * valB;
    float knn = (1 - valA) * (1 - valB);

    Face face = getFace(dim, LowPole);

    float valMin =  kxx * face.v11->values[dim] + kxn * face.v10->values[dim] +
                    knx * face.v01->values[dim] + knn * face.v00->values[dim];

    face = getFace(dim, HighPole);

    float valMax =  kxx * face.v11->values[dim] + kxn * face.v10->values[dim] +
                    knx * face.v01->values[dim] + knn * face.v00->values[dim];

    float diff = valMax - valMin;

    float progress = diff == 0.f ? 0.f : fabsf(morph[dim] - valMin) / diff;
    NumberUtils::constrain(progress, 0.f, 1.f);

    return progress;
}

bool TrilinearCube::isDeformed() const {
    for(int i = 0; i < Vertex::numElements; ++i) {
        if(pathAt(i) >= 0) {
            return true;
        }
    }

    return false;
}

void TrilinearCube::Face::set(int index, Vertex* vert) {
    switch (index) {
        case 0: v00 = vert; break;
        case 1: v01 = vert; break;
        case 2: v10 = vert; break;
        case 3: v11 = vert; break;
        default: throw std::invalid_argument("TrilinearCube::Face::set: index out of range");
    }
}

Array<Vertex*> TrilinearCube::Face::toArray() const {
    Vertex* verts[] = { v00, v01, v10, v11 };

    return {verts, 4};
}

bool TrilinearCube::Face::merge(TrilinearCube const* cube, float pos) {
    Face a = cube->getFace(dim, LowPole);
    Face b = cube->getFace(dim, HighPole);

    bool overlaps = false;
    overlaps |= dimensionsAt(pos, dim, a.v00, b.v00, v00);
    overlaps |= dimensionsAt(pos, dim, a.v01, b.v01, v01);
    overlaps |= dimensionsAt(pos, dim, a.v10, b.v10, v10);
    overlaps |= dimensionsAt(pos, dim, a.v11, b.v11, v11);

    return overlaps;
}

Vertex* TrilinearCube::Face::operator[](const int index) const {
    switch (index) {
        case 0: return v00;
        case 1: return v01;
        case 2: return v10;
        case 3: return v11;
        default: throw std::invalid_argument("TrilinearCube::Face::[]: index out of range");;
    }
}

void TrilinearCube::Face::removeOwner(TrilinearCube* cube) {
    for(int i = 0; i < size(); ++i) {
        (*this)[i]->removeOwner(cube);
    }
}
