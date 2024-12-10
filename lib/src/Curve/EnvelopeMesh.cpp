#include "EnvelopeMesh.h"
#include "Vertex.h"
#include "../Obj/MorphPosition.h"
#include "VertCube.h"
#include "../Definitions.h"

EnvelopeMesh::EnvelopeMesh(const String& name) :
		Mesh(name) {
}

float EnvelopeMesh::getPositionOfCubeAt(VertCube* line, MorphPosition pos) {
	if(line == nullptr)
		return -1.f;

	pos.time = 0;
	VertCube::ReductionData reduce;
	line->getFinalIntercept(reduce, pos);

	if(reduce.pointOverlaps)
		return reduce.v[Vertex::Phase];

	return -1.f;
}

void EnvelopeMesh::writeXML(XmlElement* envLayersElem) const {
  #ifndef DEMO_VERSION
	XmlElement* envMeshElem 	= new XmlElement("EnvelopeMesh");
	XmlElement* mainMeshElem 	= new XmlElement("MainMesh");
	XmlElement* loopIndicesElem	= new XmlElement("LoopIndices");
	XmlElement* sustIndicesElem	= new XmlElement("SustIndices");

	Mesh::writeXML(mainMeshElem);

    for (int i = 0; i < cubes.size(); ++i) {
        VertCube* line = cubes[i];

        if (line == nullptr)
            continue;

        if (loopCubes.find(line) != loopCubes.end()) {
			XmlElement* loopIndexElem = new XmlElement("LoopIndex");
			loopIndexElem->setAttribute("index", i);
			loopIndicesElem->addChildElement(loopIndexElem);
		}

        if (sustainCubes.find(line) != sustainCubes.end()) {
			XmlElement* sustIndexElem = new XmlElement("SustIndex");
			sustIndexElem->setAttribute("index", i);
			sustIndicesElem->addChildElement(sustIndexElem);
		}
	}

	envMeshElem->setAttribute("name", name);
	envMeshElem->addChildElement(mainMeshElem);
	envMeshElem->addChildElement(loopIndicesElem);
	envMeshElem->addChildElement(sustIndicesElem);
	envLayersElem->addChildElement(envMeshElem);
  #endif
}


bool EnvelopeMesh::readXML(const XmlElement* envLayersElem) {
	XmlElement* envMeshElem = envLayersElem->getChildByName("EnvelopeMesh");
	if(envMeshElem == nullptr)
		return false;

	XmlElement* mainMeshElem 	= envMeshElem->getChildByName("MainMesh");

	if(mainMeshElem != nullptr)
		Mesh::readXML(mainMeshElem);

	XmlElement* sustIndicesElem = envMeshElem->getChildByName("SustIndices");
	XmlElement* loopIndicesElem = envMeshElem->getChildByName("LoopIndices");

    if (loopIndicesElem != nullptr) {
        loopCubes.clear();
        forEachXmlChildElementWithTagName(*loopIndicesElem, loopIndexElem, "LoopIndex")
        {
            int index = loopIndexElem->getIntAttribute("index", -1);

            if (isPositiveAndBelow(index, (int) cubes.size())) {
				clampSharpness(cubes[index]);
				loopCubes.insert(cubes[index]);
			}
        }
    } else {
        int loopIndex = envMeshElem->getIntAttribute("sustainIndex", -1);

        if (isPositiveAndBelow(loopIndex, (int) cubes.size())) {
			clampSharpness(cubes[loopIndex]);
			loopCubes.insert(cubes[loopIndex]);
		}
	}

    if (sustIndicesElem != nullptr) {
        sustainCubes.clear();
        forEachXmlChildElementWithTagName(*sustIndicesElem, sustIndexElem, "SustIndex") {
            int index = sustIndexElem->getIntAttribute("index", -1);

            if (isPositiveAndBelow(index, (int) cubes.size())) {
				clampSharpness(cubes[index]);
				sustainCubes.insert(cubes[index]);
			}
		}
    } else {
        int sustainIndex = envMeshElem->getIntAttribute("sustainIndex2", -1);

        if (isPositiveAndBelow(sustainIndex, (int) cubes.size())) {
            clampSharpness(cubes[sustainIndex]);
			sustainCubes.insert(cubes[sustainIndex]);
		}
	}

	if(sustainCubes.empty() && ! cubes.empty())	{
		setSustainToRightmost();
	}

	return true;
}

void EnvelopeMesh::setSustainToLast() {
    sustainCubes.insert(cubes.back());
}

void EnvelopeMesh::destroy() {
    sustainCubes.clear();
    loopCubes.clear();

    Mesh::destroy();
}

void EnvelopeMesh::clampSharpness(VertCube* line) {
    if (line->getCompDfrm() >= 0) {
	    return;
    }

    for (int i = 0; i < VertCube::numVerts; ++i) {
	    line->getVertex(i)->setMaxSharpness();
    }
}

void EnvelopeMesh::setSustainToRightmost() {
    sustainCubes.clear();

	set<float> blueSlicePoints, redSlicePoints;

	for(auto line : cubes) {
        for (int j = 0; j < 4; ++j) {
			blueSlicePoints.insert(line->lineVerts[j]->values[Vertex::Blue]);
			redSlicePoints.insert(line->lineVerts[j]->values[Vertex::Red]);
		}
	}

	VertCube::ReductionData reduce;

	for(float blueSlicePoint : blueSlicePoints) {
        float blueSlice = jmin(1.f, blueSlicePoint + 0.0001f);

		for(float redSlicePoint : redSlicePoints) {

            // increase by a fraction so that overlap test finds line in front
            float redSlice = jmin(1.f, redSlicePoint + 0.0001f);
            float maxX = -1;

            VertCube* maxLine = nullptr;

            foreach(CubeIter, cit, cubes) {
                VertCube* line = *cit;

                if (line == nullptr)
                    continue;

                line->getFinalIntercept(reduce, MorphPosition(0, redSlice, blueSlice));

                if (reduce.pointOverlaps) {
                    float x = reduce.v.values[Vertex::Phase];

					if(x > maxX) {
                        maxLine = line;
						maxX = x;
					}
				}
			}

            if (maxLine != nullptr) {
				clampSharpness(maxLine);
				sustainCubes.insert(maxLine);
			}
		}
	}
}
