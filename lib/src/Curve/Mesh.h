#pragma once
#include <vector>
#include "../App/Doc/Savable.h"

using std::vector;

class VertCube;
class Vertex;

class Mesh : public Savable {
public:

    Mesh();
    explicit Mesh(String  name);

    bool hasEnoughCubesForCrossSection();
    void print(bool printCubes = true, bool printVerts = true);
    void validate();
    void removeFreeVerts();
    void updateToVersion(double newVersion);
    void twin(float padLeft, float padRight);

    virtual void destroy();
    virtual void deepCopy(Mesh* mesh);
    void writeXML(XmlElement* element) const override;
    bool readXML(const XmlElement* element) override;

    vector<Vertex*>& getVerts()   { return verts; }
    vector<VertCube*>& getCubes() { return cubes; }

    [[nodiscard]] const String& getName() const { return name; }
    [[nodiscard]] int getNumCubes() const 	 { return (int) cubes.size(); }
    [[nodiscard]] int getNumVerts() const 	 { return (int) verts.size(); }

    void addCube(VertCube* cube) { cubes.insert(cubes.end(), cube); }
    void addVertex(Vertex* vert) { verts.insert(verts.end(), vert); }

    bool removeCube(VertCube* cube) {
        return removeCube(std::find(cubes.begin(), cubes.end(), cube));
    }

    bool removeCube(vector<VertCube*>::iterator cube) {
        if(cube == cubes.end())
            return false;

        cubes.erase(cube);
        return true;
    }

    bool removeVert(Vertex* vert) {
        return removeVert(std::find(verts.begin(), verts.end(), vert));
    }

    bool removeVert(vector<Vertex*>::iterator vert) {
        if(vert == verts.end())
            return false;

        verts.erase(vert);
        return true;
    }

    void copyElements(vector<VertCube*>& copyCubesTo) const	{ copyCubesTo = cubes; }
    void copyElements(vector<Vertex*>& copyVertsTo) const	{ copyVertsTo = verts; }

    void copyElements(vector<Vertex*>& copyVertsTo, vector<VertCube*>& copyCubesTo) const {
        copyElements(copyVertsTo);
        copyElements(copyCubesTo);
    }

protected:
    int version;
    String name;

    vector<Vertex*> verts;
    vector<VertCube*> cubes;

    JUCE_LEAK_DETECTOR(Mesh)
};