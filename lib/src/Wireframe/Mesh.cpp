#include <map>
#include <iostream>
#include <utility>
#include "Interpolator/Trilinear/TrilinearVertex.h"
#include "Interpolator/Trilinear/TrilinearCube.h"
#include "Mesh.h"

#include <App/AppConstants.h>

#include "../Definitions.h"

using std::map;

Mesh::Mesh() :
        name("unnamed")
    ,   version(Constants::MeshFormatVersion) {
}

Mesh::Mesh(String  name) :
        name(std::move(name))
    ,   version(Constants::MeshFormatVersion) {
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
        info("Lines: " << (int) cubes.size() << "\n");

        for(auto it : cubes) {
            TrilinearCube& cube = *it;

            std::cout << String::formatted("%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\n%3.4f\t%3.4f\t%3.4f\t%3.4f\t%3.4f\t%3.4f\n",
                   cube.pathChans[Vertex::Red],
                   cube.pathChans[Vertex::Blue],
                   cube.pathChans[Vertex::Phase],
                   cube.pathChans[Vertex::Amp],
                   cube.pathChans[Vertex::Curve],
                   cube.pathChans[Vertex::Time],
                   cube.pathGains[Vertex::Red],
                   cube.pathGains[Vertex::Blue],
                   cube.pathGains[Vertex::Phase],
                   cube.pathGains[Vertex::Amp],
                   cube.pathGains[Vertex::Curve],
                   cube.pathGains[Vertex::Time]
            );

            for (int i = 0; i < TrilinearCube::numVerts; ++i) {
                std::cout << String::formatted("%p\t%3.6f\t%3.6f\t%3.6f\t%3.6f\t%3.6f\t%3.6f\n", &cube,
                       cube.getVertex(i)->values[0],
                       cube.getVertex(i)->values[1],
                       cube.getVertex(i)->values[2],
                       cube.getVertex(i)->values[3],
                       cube.getVertex(i)->values[4],
                       cube.getVertex(i)->values[5]
                   );
            }

            info("\n");
        }

        info("\n");
    }

    if (printVerts) {
        info("Verts: " << (int) verts.size() << "\n");
        int index = 1;
        for(auto* vert : verts) {
            std::cout << String::formatted("%d.\t%3.6f\t%3.6f\t%3.6f\t%3.6f\t%3.6f",
                                      index, vert->values[0], vert->values[1], vert->values[2], vert->values[3],
                                      vert->values[4]);

            for(auto& cube : vert->owners) {
                info("\t" << (int64) cube);
            }

            info("\n");
            ++index;
        }
        info("\n");
    }
#endif
}


void Mesh::writeXML(XmlElement* parentElem) const {
    auto* meshElem = new XmlElement("Mesh");
    parentElem->addChildElement(meshElem);

    meshElem->setAttribute("name",      name);
    meshElem->setAttribute("version",   Constants::MeshFormatVersion);

    int count = 0;
    map<Vertex*, int> idMap;
    for(auto* vert : verts) {
        auto* vertElem = new XmlElement("Vertex");

        vertElem->setAttribute("time",   vert->values[Vertex::Time] );
        vertElem->setAttribute("phase",  vert->values[Vertex::Phase]);
        vertElem->setAttribute("amp",    vert->values[Vertex::Amp]  );
        vertElem->setAttribute("key",    vert->values[Vertex::Red]  );
        vertElem->setAttribute("mod",    vert->values[Vertex::Blue] );
        vertElem->setAttribute("weight", vert->values[Vertex::Curve]);

        vertElem->setAttribute("id", count);

        idMap[vert] = count;
        meshElem->addChildElement(vertElem);

        ++count;
    }

    for(auto* cube : cubes) {
        auto* cubeElem = new XmlElement("TrilinearCube");

        for (int i = 0; i < TrilinearCube::numVerts; ++i) {
            auto* vertElem = new XmlElement("Vertex");
            vertElem->setAttribute("lineVertexNumber", i);
            vertElem->setAttribute("vertexId", idMap[cube->getVertex(i)]);

            cubeElem->addChildElement(vertElem);
        }

        jassert(cube->pathGainAt(Vertex::Curve) >= 0);

        cubeElem->setAttribute("avpGuide",  cube->pathChans[Vertex::Time]   );
        cubeElem->setAttribute("keyGuide",  cube->pathChans[Vertex::Red]    );
        cubeElem->setAttribute("modGuide",  cube->pathChans[Vertex::Blue]   );
        cubeElem->setAttribute("phaseGuide",cube->pathChans[Vertex::Phase]  );
        cubeElem->setAttribute("ampGuide",  cube->pathChans[Vertex::Amp]    );
        cubeElem->setAttribute("curveGuide",cube->pathChans[Vertex::Curve]  );

        cubeElem->setAttribute("avpGain",   cube->pathGains[Vertex::Time]   );
        cubeElem->setAttribute("keyGain",   cube->pathGains[Vertex::Red]    );
        cubeElem->setAttribute("modGain",   cube->pathGains[Vertex::Blue]   );
        cubeElem->setAttribute("phaseGain", cube->pathGains[Vertex::Phase]  );
        cubeElem->setAttribute("ampGain",   cube->pathGains[Vertex::Amp]    );
        cubeElem->setAttribute("curveGain", cube->pathGains[Vertex::Curve]  );

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
        vert->values[Vertex::Time]  = currentVert->getDoubleAttribute("time"    );
        vert->values[Vertex::Phase] = currentVert->getDoubleAttribute("phase"   );
        vert->values[Vertex::Amp]   = currentVert->getDoubleAttribute("amp"     );
        vert->values[Vertex::Red]   = currentVert->getDoubleAttribute("key"     );
        vert->values[Vertex::Blue]  = currentVert->getDoubleAttribute("mod"     );
        vert->values[Vertex::Curve] = currentVert->getDoubleAttribute("weight"  );

        int id = currentVert->getIntAttribute("id");

        idMap[id] = vert;
        verts.push_back(vert);
    }

    vector<Vertex*> failedVerts;
    vector<TrilinearCube*> failedLines;

    for(auto currentCube : meshElem->getChildWithTagNameIterator("TrilinearCube")) {
        auto* cube = new TrilinearCube();

        cube->pathGains[Vertex::Amp]   = currentCube->getDoubleAttribute("ampGain",     0.5);
        cube->pathGains[Vertex::Blue]  = currentCube->getDoubleAttribute("modGain",     0.5);
        cube->pathGains[Vertex::Curve] = currentCube->getDoubleAttribute("curveGain",   0.5);
        cube->pathGains[Vertex::Phase] = currentCube->getDoubleAttribute("phaseGain",   0.5);
        cube->pathGains[Vertex::Red]   = currentCube->getDoubleAttribute("keyGain",     0.5);
        cube->pathGains[Vertex::Time]  = currentCube->getDoubleAttribute("avpGain",     0.5);

        cube->pathChans[Vertex::Amp]   = currentCube->getIntAttribute("ampGuide",   -1);
        cube->pathChans[Vertex::Blue]  = currentCube->getIntAttribute("modGuide",   -1);
        cube->pathChans[Vertex::Curve] = currentCube->getIntAttribute("curveGuide", -1);
        cube->pathChans[Vertex::Phase] = currentCube->getIntAttribute("phaseGuide", -1);
        cube->pathChans[Vertex::Red]   = currentCube->getIntAttribute("keyGuide",   -1);
        cube->pathChans[Vertex::Time]  = currentCube->getIntAttribute("avpGuide",   -1);

        if(cube->getCompPath() < 0) {
            cube->getCompPath() = currentCube->getIntAttribute("timeGuide",  -1);
        }

        int numVertsSet = 0;
        bool failed = false;

        for(auto currentVert : currentCube->getChildWithTagNameIterator("Vertex")) {
            if (currentVert == nullptr) {
                jassertfalse;
                return false;
            }

            int num = currentVert->getIntAttribute("lineVertexNumber",  -1);
            int id  = currentVert->getIntAttribute("vertexId",          -1);

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

        jassert(numVertsSet == TrilinearCube::numVerts);

        if(failed) {
            for (int i = 0; i < TrilinearCube::numVerts; ++i) {
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
            vert->values[Vertex::Amp]   = vert->values[Vertex::Phase];
            vert->values[Vertex::Phase] = vert->values[Vertex::Time];
            vert->values[Vertex::Time]  = 0;
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
