#pragma once
#include <vector>
#include "../App/Doc/Savable.h"

using std::vector;

class VertCube;
class Vertex;

typedef vector<Vertex*> 	VertList;
typedef vector<VertCube*>	CubeList;

typedef VertList::const_iterator 	ConstVertIter;
typedef CubeList::const_iterator 	ConstCubeIter;

typedef VertList::iterator 	VertIter;
typedef CubeList::iterator CubeIter;

class Mesh : public Savable {
public:

    Mesh();
    explicit Mesh(String  name);
	~Mesh() override;

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

	const String& getName() const { return name; }

	VertList& getVerts() 		 { return verts; }
	CubeList& getCubes() 		 { return cubes; }

	int getNumCubes() const 	 { return (int) cubes.size(); }
	int getNumVerts() const 	 { return (int) verts.size(); }

	void addCube(VertCube* cube) { cubes.insert(cubes.end(), cube); }
	void addVertex(Vertex* vert) { verts.insert(verts.end(), vert); }

    bool removeCube(VertCube* cube) {
        return removeCube(std::find(cubes.begin(), cubes.end(), cube));
    }

    bool removeCube(CubeIter cube) {
		if(cube == cubes.end())
			return false;

		cubes.erase(cube);
		return true;
	}

    bool removeVert(Vertex* vert) {
        return removeVert(std::find(verts.begin(), verts.end(), vert));
    }

    bool removeVert(VertIter vert) {
		if(vert == verts.end())
			return false;

		verts.erase(vert);
		return true;
	}

	void copyElements(CubeList& copyCubesTo) const	{ copyCubesTo = cubes; }
	void copyElements(VertList& copyVertsTo) const 	{ copyVertsTo = verts; }

	void copyElements(VertList& copyVertsTo, CubeList& copyCubesTo) const
	{
		copyElements(copyVertsTo);
		copyElements(copyCubesTo);
	}

	CubeIter getCubeStart() 				{ return cubes.begin(); }
	CubeIter getCubeEnd() 					{ return cubes.end(); 	}

	ConstCubeIter getConstCubeStart() const { return cubes.begin(); }
	ConstCubeIter getConstCubeEnd() const	{ return cubes.end(); 	}

	VertIter getVertStart() 				{ return verts.begin(); }
	VertIter getVertEnd() 					{ return verts.end(); 	}

	ConstVertIter getConstVertStart() const	{ return verts.begin(); }
	ConstVertIter getConstVertEnd() const	{ return verts.end(); 	}

protected:
	double version;
	String name;

	VertList verts;
	CubeList cubes;

	JUCE_LEAK_DETECTOR(Mesh)
};