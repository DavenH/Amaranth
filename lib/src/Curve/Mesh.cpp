#include <map>
#include <iostream>
#include <utility>
#include "Vertex.h"
#include "VertCube.h"
#include "Mesh.h"
#include "../Definitions.h"

using std::map;

Mesh::Mesh() :
        name("unnamed")
    , 	version(2.0) {
}

Mesh::Mesh(String  name) :
        name(std::move(name))
    , 	version(2.0) {
}

void Mesh::destroy() {
    // lines should be deleted first as they try to remove the vertices' references
    // before destruction.
    if (!cubes.empty()) {
        for(auto& cube : cubes) {
            delete cube;
            cube = nullptr;
        }

        cubes.clear();
    }

    if (!verts.empty()) {
        for(auto& vert : verts) {
            delete vert;
            vert = nullptr;
        }

        verts.clear();
    }
}

void Mesh::print(bool printLines, bool printVerts) {
#ifndef ORGANIZER
    if (printLines) {
        std::cout << "Lines: " << (int) cubes.size() << "\n";

        for(auto it : cubes) {
            VertCube& cube = *it;

            std::cout << String::formatted("%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\n%3.4f\t%3.4f\t%3.4f\t%3.4f\t%3.4f\t%3.4f\n",
                   cube.dfrmChans[Vertex::Red],
                   cube.dfrmChans[Vertex::Blue],
                   cube.dfrmChans[Vertex::Phase],
                   cube.dfrmChans[Vertex::Amp],
                   cube.dfrmChans[Vertex::Curve],
                   cube.dfrmChans[Vertex::Time],
                   cube.dfrmGains[Vertex::Red],
                   cube.dfrmGains[Vertex::Blue],
                   cube.dfrmGains[Vertex::Phase],
                   cube.dfrmGains[Vertex::Amp],
                   cube.dfrmGains[Vertex::Curve],
                   cube.dfrmGains[Vertex::Time]
            );

            for (int i = 0; i < VertCube::numVerts; ++i) {
                std::cout << String::formatted("%p\t%3.6f\t%3.6f\t%3.6f\t%3.6f\t%3.6f\t%3.6f\n", &cube,
                       cube.getVertex(i)->values[0],
                       cube.getVertex(i)->values[1],
                       cube.getVertex(i)->values[2],
                       cube.getVertex(i)->values[3],
                       cube.getVertex(i)->values[4],
                       cube.getVertex(i)->values[5]
                   );
            }

            std::cout << "\n";
        }

        std::cout << "\n";
    }

    if (printVerts) {
        std::cout << "Verts: " << (int) verts.size() << "\n";
        int index = 1;
        for(auto* vert : verts) {
            std::cout << String::formatted("%d.\t%3.6f\t%3.6f\t%3.6f\t%3.6f\t%3.6f",
                                      index, vert->values[0], vert->values[1], vert->values[2], vert->values[3],
                                      vert->values[4]);

            for(auto& cube : vert->owners) {
                std::cout << "\t" << (int64) cube;
            }

            std::cout << "\n";
            ++index;
        }
        std::cout << "\n";
    }
#endif
}


void Mesh::writeXML(XmlElement* parentElem) const {
    auto* meshElem = new XmlElement("Mesh");
    parentElem->addChildElement(meshElem);

    meshElem->setAttribute("name", 		name);
    meshElem->setAttribute("version", 	2.0);

    int count = 0;
    map<Vertex*, int> idMap;
    for(auto* vert : verts) {
        auto* vertElem = new XmlElement("Vertex");

        vertElem->setAttribute("time", 	 vert->values[Vertex::Time]	);
        vertElem->setAttribute("phase",  vert->values[Vertex::Phase]	);
        vertElem->setAttribute("amp", 	 vert->values[Vertex::Amp]	);
        vertElem->setAttribute("key", 	 vert->values[Vertex::Red]	);
        vertElem->setAttribute("mod", 	 vert->values[Vertex::Blue]	);
        vertElem->setAttribute("weight", vert->values[Vertex::Curve]	);

        vertElem->setAttribute("id", count);

        idMap[vert] = count;
        meshElem->addChildElement(vertElem);

        ++count;
    }

    for(auto* cube : cubes) {
        auto* cubeElem = new XmlElement("VertCube");

        for (int i = 0; i < VertCube::numVerts; ++i) {
            auto* vertElem = new XmlElement("Vertex");
            vertElem->setAttribute("lineVertexNumber", i);
            vertElem->setAttribute("vertexId", idMap[cube->getVertex(i)]);

            cubeElem->addChildElement(vertElem);
        }

        jassert(cube->dfrmGainAt(Vertex::Curve) >= 0);

        cubeElem->setAttribute("avpGuide", 	cube->dfrmChans[Vertex::Time]	);
        cubeElem->setAttribute("keyGuide", 	cube->dfrmChans[Vertex::Red]	);
        cubeElem->setAttribute("modGuide", 	cube->dfrmChans[Vertex::Blue]	);
        cubeElem->setAttribute("phaseGuide",cube->dfrmChans[Vertex::Phase]	);
        cubeElem->setAttribute("ampGuide", 	cube->dfrmChans[Vertex::Amp]	);
        cubeElem->setAttribute("curveGuide",cube->dfrmChans[Vertex::Curve]	);

        cubeElem->setAttribute("avpGain",	cube->dfrmGains[Vertex::Time]	);
        cubeElem->setAttribute("keyGain",	cube->dfrmGains[Vertex::Red]	);
        cubeElem->setAttribute("modGain",	cube->dfrmGains[Vertex::Blue]	);
        cubeElem->setAttribute("phaseGain",	cube->dfrmGains[Vertex::Phase]	);
        cubeElem->setAttribute("ampGain",	cube->dfrmGains[Vertex::Amp]	);
        cubeElem->setAttribute("curveGain",	cube->dfrmGains[Vertex::Curve]	);

        meshElem->addChildElement(cubeElem);
    }
}

bool Mesh::readXML(const XmlElement* repoElem) {
    if (repoElem == nullptr)
        return false;

    XmlElement* meshElem = repoElem->getChildByName("Mesh");

    if (meshElem == nullptr) {
        jassertfalse;
        return false;
    }

    version = meshElem->getDoubleAttribute("version", 1);
    name = meshElem->getStringAttribute("name", "unnamed");

    map<int, Vertex*> idMap;

    for(auto currentVert : meshElem->getChildWithTagNameIterator("Vertex")) {
        if (currentVert == nullptr) {
            jassertfalse;
            return false;
        }

        auto* vert = new Vertex();
        vert->values[Vertex::Time] 	= currentVert->getDoubleAttribute("time"	);
        vert->values[Vertex::Phase] = currentVert->getDoubleAttribute("phase"	);
        vert->values[Vertex::Amp] 	= currentVert->getDoubleAttribute("amp"		);
        vert->values[Vertex::Red] 	= currentVert->getDoubleAttribute("key"		);
        vert->values[Vertex::Blue] 	= currentVert->getDoubleAttribute("mod"		);
        vert->values[Vertex::Curve] = currentVert->getDoubleAttribute("weight"	);

        int id = currentVert->getIntAttribute("id");

        idMap[id] = vert;
        verts.push_back(vert);
    }

    vector<Vertex*> failedVerts;
    vector<VertCube*> failedLines;

    for(auto currentCube : meshElem->getChildWithTagNameIterator("VertCube")) {
        auto* cube = new VertCube();

        cube->dfrmGains[Vertex::Amp]   = currentCube->getDoubleAttribute("ampGain", 	0.5);
        cube->dfrmGains[Vertex::Blue]  = currentCube->getDoubleAttribute("modGain", 	0.5);
        cube->dfrmGains[Vertex::Curve] = currentCube->getDoubleAttribute("curveGain", 	0.5);
        cube->dfrmGains[Vertex::Phase] = currentCube->getDoubleAttribute("phaseGain", 	0.5);
        cube->dfrmGains[Vertex::Red]   = currentCube->getDoubleAttribute("keyGain", 	0.5);
        cube->dfrmGains[Vertex::Time]  = currentCube->getDoubleAttribute("avpGain", 	0.5);

        cube->dfrmChans[Vertex::Amp]   = currentCube->getIntAttribute("ampGuide", 	-1);
        cube->dfrmChans[Vertex::Blue]  = currentCube->getIntAttribute("modGuide",   -1);
        cube->dfrmChans[Vertex::Curve] = currentCube->getIntAttribute("curveGuide", -1);
        cube->dfrmChans[Vertex::Phase] = currentCube->getIntAttribute("phaseGuide", -1);
        cube->dfrmChans[Vertex::Red]   = currentCube->getIntAttribute("keyGuide",   -1);
        cube->dfrmChans[Vertex::Time]  = currentCube->getIntAttribute("avpGuide",   -1);

        if(cube->getCompDfrm() < 0) {
            cube->getCompDfrm() = currentCube->getIntAttribute("timeGuide",  -1);
        }

        int numVertsSet = 0;
        bool failed = false;

        for(auto currentVert : currentCube->getChildWithTagNameIterator("Vertex")) {
            if (currentVert == nullptr) {
                jassertfalse;
                return false;
            }

            int num = currentVert->getIntAttribute("lineVertexNumber", 	-1);
            int id 	= currentVert->getIntAttribute("vertexId", 			-1);

            if(num < 0 || id < 0) {
                failed = true;
                break;
            }

            Vertex* vert = idMap[id];
            if (cube->setVertex(vert, num)) {
                ++numVertsSet;
            } else {
                failed = true;
                break;
            }
        }

        jassert(numVertsSet == VertCube::numVerts);

        if(failed) {
            for (int i = 0; i < VertCube::numVerts; ++i) {
                Vertex* vert = cube->getVertex(i);

                if(vert != nullptr) {
                    failedVerts.push_back(vert);
                }
            }

            failedLines.push_back(cube);
        } else {
            cubes.push_back(cube);
        }
    }

    validate();

    return true;
}

bool Mesh::hasEnoughCubesForCrossSection() {
    return cubes.size() > 1;
}

void Mesh::validate() {
    for(auto* vert : verts) {
        vert->owners.clear();
    }

    for(auto* cube : cubes) {
        for(auto& lineVert : cube->lineVerts) {
            lineVert->addOwner(cube);
        }

        cube->validate();
    }
}

void Mesh::removeFreeVerts() {
    vector<Vertex*> vertsToDelete;

    for(auto* vert : verts) {
        if (vert->owners.size() == 0) {
            vertsToDelete.push_back(vert);
        }
    }

    for(auto* vert : vertsToDelete) {
        removeVert(vert);
    }
}

void Mesh::updateToVersion(double newVersion) {
    if (version >= 1 && version < 1.1) {
        for(auto* vert : verts) {
            vert->values[Vertex::Amp] 	= vert->values[Vertex::Phase];
            vert->values[Vertex::Phase] = vert->values[Vertex::Time];
            vert->values[Vertex::Time] 	= 0;
        }

        version = newVersion;
    }
}

void Mesh::deepCopy(Mesh* mesh) {
    destroy();

    std::unique_ptr<XmlElement> elem(new XmlElement("TopElement"));
    mesh->writeXML(elem.get());
    readXML(elem.get());
}

void Mesh::twin(float padLeft, float padRight) {
    float scale = (1.f - padLeft - padRight) * 0.5f;

    std::unique_ptr<Mesh> meshCopy(new Mesh());
    meshCopy->deepCopy(this);

    vector<Vertex*> toDelete;

    for(auto* vert : verts) {
        float& phase = vert->values[Vertex::Phase];

        if(phase < padLeft || phase > 1.f - padRight) {
            continue;
        }

        phase = (phase - padLeft) * 0.5 + padLeft;
    }

    for(auto* vert : meshCopy->getVerts()) {
        float& phase = vert->values[Vertex::Phase];

        if (phase < padLeft || phase >= 1.f - padRight) {
            toDelete.push_back(vert);
            continue;
        }

        phase = (phase - padLeft) * 0.5 + padLeft + scale;

        verts.push_back(vert);
    }

    for(auto*& vert : toDelete) {
        delete vert;
        vert = nullptr;
    }

    cubes.insert(cubes.end(), meshCopy->cubes.begin(), meshCopy->cubes.end());
}
