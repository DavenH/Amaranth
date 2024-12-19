#pragma once

#include <algorithm>
#include "JuceHeader.h"
#include "Vertex.h"

class Mesh;
class MorphPosition;

class VertCube {
public:
    static const size_t numVerts = 8;

    /* nb: poles do not necessarily correspond with low or high values
     *	   but are merely an objective distinction between to sides of
     *     the line cube along a particular dimension
     */
    enum Pole { LowPole, HighPole };

    enum {
        y0r0b0,	y0r0b1,
        y0r1b0,	y0r1b1,
        y1r0b0,	y1r0b1,
        y1r1b0,	y1r1b1,
    };

    struct ReductionData {
        bool pointOverlaps, lineOverlaps;
        Vertex v, v0, v1, v00, v01, v10, v11;

        Vertex& operator[](const int index) { return index == 0 ? v0 : v1; }
    };

    struct Face {
        int dim;
        Vertex *v00, *v01, *v10, *v11;

        explicit Face(int dim) : dim(dim), v00(nullptr),   v01(nullptr),   v10(nullptr),   v11(nullptr)   {}
        Face(ReductionData& data, int dim) 	: dim(dim), v00(&data.v00), v01(&data.v01), v10(&data.v10), v11(&data.v11) {}

        bool merge(VertCube const* cube, float pos);
        void removeOwner(VertCube* cube);
        void set(int index, Vertex* vert);
        [[nodiscard]] Array<Vertex*> toArray() const;
        Vertex* operator[] (int index) const;
        int size() { return numVerts / 2; }
    };

    struct Edge {
        int dim{};
        Vertex *v0, *v1;

        Edge() : v0(nullptr), v1(nullptr) {}
        Edge(Vertex* a, Vertex* b) : v0(a), v1(b) {}
        Vertex* operator[] (const int index) const { return index == 0 ? v0 : v1; }
        int size() { return numVerts / 2; }
    };

    /* ----------------------------------------------------------------------------- */

    VertCube();
    explicit VertCube(Mesh* mesh);
    VertCube(VertCube& cube);
    ~VertCube();

    bool intersectsMorphRect(int dim, ReductionData& data, const MorphPosition& pos) 	const;
    void getFinalIntercept	(ReductionData& data, const MorphPosition& pos) 			const;
    void getInterceptsFast	(int xDim, ReductionData& data, const MorphPosition& pos) 	const;
    void getPoles			(Vertex const* vert, bool& time, bool& red, bool& blue) 	const;

    void init();
    void initVerts(const MorphPosition& pos);
    void orphanVerts();
    void resetProperties();
    void setPropertiesFrom(VertCube const* other);
    void validate();
    void setFace(const Face& face, int xDim, bool pole);
    bool setVertex(Vertex* vertex, int index);

    [[nodiscard]] bool isDeformed() const;
    bool poleOf(int dim, Vertex const* poleVert) const;
    bool hasCommonVertsWith(VertCube const* cube) const;
    bool hasCommonEdgeAlong(int dim, VertCube const* cube) const;
    [[nodiscard]] bool containsEdge(const Edge& edge) const;
    Array<VertCube*> getAllAdjacentCubes(int dim, const MorphPosition& pos);
    [[nodiscard]] VertCube* getAdjacentCube(int dim, bool pole) const;
    int indexOf(Vertex const* vertex) const;
    [[nodiscard]] float deformerAbsGain(int dim) const;
    [[nodiscard]] float getPortionAlong(int dim, const MorphPosition& morph) const;
    [[nodiscard]] Face getFace(int xDim, bool pole) const;
    Face getFace(int xDim, Vertex const* pole) const;
    [[nodiscard]] MorphPosition getCentroid(bool isEnv = false) const;
    [[nodiscard]] Vertex* findClosestVertex(const MorphPosition& pos) const;
    [[nodiscard]] Vertex* getVertex(int index) const;
    [[nodiscard]] Vertex* getOtherVertexAlong(int dim, Vertex const* vert) const;
    [[nodiscard]] Vertex* getVertWithPolarities(bool time, bool red, bool blue) const;
    [[nodiscard]] Array<Vertex*> toArray() const;

    void getMultidimIntercept(Vertex* a1, Vertex* a2,
                              Vertex* b1, Vertex* b2,
                              Vertex* v1, Vertex* v2,
                              float& redLow, float& redHigh,
                              float& blueLow, float& blueHigh,
                              const MorphPosition& pos) const;

    void getInterceptsAccurate(int primeDim,
            ReductionData& data,
            const MorphPosition& pos) const;

    char& deformerAt(int dim) { return dfrmChans[dim]; }
    float& dfrmGainAt(int dim) { return dfrmGains[dim]; }

    [[nodiscard]] const char& deformerAt(int dim) const 	{ return dfrmChans[dim]; }
    [[nodiscard]] const float& dfrmGainAt(int dim) const 	{ return dfrmGains[dim]; }

    char& getCompDfrm() { return deformerAt(Vertex::Time); }
    float getCompGain() { return dfrmGainAt(Vertex::Time); }

    static void getPoles(int index, bool& time, bool& red, bool& blue);
    static bool dimensionsAt(float x, int dim, Vertex const* one, Vertex const* two, Vertex* vertex);
    static void positionAt(float x, int axis, Vertex const* one, Vertex const* two, Vertex* vertex);
    static void vertexAt(float x, int vAxis, Vertex const* one, Vertex const* two, Vertex* vertex);

    VertCube(const VertCube& that) {
        operator=(that);
    }

    VertCube& operator=(const VertCube& that) {
        for (int i = 0; i <= Vertex::Curve; ++i) {
            dfrmChans[i] = that.dfrmChans[i];
            dfrmGains[i] = that.dfrmGains[i];
        }

        return *this;
    }

    /* ----------------------------------------------------------------------------- */

    char dfrmChans[Vertex::numElements]{};
    float dfrmGains[Vertex::numElements]{};

    Vertex* lineVerts[numVerts]{};

    JUCE_LEAK_DETECTOR(VertCube);

private:
    void getAdjacentCubes(int dim,
            Array<VertCube*>& cubes,
            ReductionData& data,
            const MorphPosition& pos);
};
