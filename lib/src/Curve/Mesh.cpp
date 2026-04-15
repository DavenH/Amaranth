#include <map>
#include <iostream>
#include <utility>
#include "Vertex.h"
#include "VertCube.h"
#include "Mesh.h"

#include <App/AppConstants.h>

#include "../App/Doc/PresetJson.h"
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
            VertCube& cube = *it;

            std::cout << String::formatted("%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\n%3.4f\t%3.4f\t%3.4f\t%3.4f\t%3.4f\t%3.4f\n",
                   cube.guideCurveChans[Vertex::Red],
                   cube.guideCurveChans[Vertex::Blue],
                   cube.guideCurveChans[Vertex::Phase],
                   cube.guideCurveChans[Vertex::Amp],
                   cube.guideCurveChans[Vertex::Curve],
                   cube.guideCurveChans[Vertex::Time],
                   cube.guideCurveGains[Vertex::Red],
                   cube.guideCurveGains[Vertex::Blue],
                   cube.guideCurveGains[Vertex::Phase],
                   cube.guideCurveGains[Vertex::Amp],
                   cube.guideCurveGains[Vertex::Curve],
                   cube.guideCurveGains[Vertex::Time]
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
        auto* cubeElem = new XmlElement("VertCube");

        for (int i = 0; i < VertCube::numVerts; ++i) {
            auto* vertElem = new XmlElement("Vertex");
            vertElem->setAttribute("lineVertexNumber", i);
            vertElem->setAttribute("vertexId", idMap[cube->getVertex(i)]);

            cubeElem->addChildElement(vertElem);
        }

        jassert(cube->guideCurveGainAt(Vertex::Curve) >= 0);

        cubeElem->setAttribute("avpGuide",  cube->guideCurveChans[Vertex::Time]   );
        cubeElem->setAttribute("keyGuide",  cube->guideCurveChans[Vertex::Red]    );
        cubeElem->setAttribute("modGuide",  cube->guideCurveChans[Vertex::Blue]   );
        cubeElem->setAttribute("phaseGuide",cube->guideCurveChans[Vertex::Phase]  );
        cubeElem->setAttribute("ampGuide",  cube->guideCurveChans[Vertex::Amp]    );
        cubeElem->setAttribute("curveGuide",cube->guideCurveChans[Vertex::Curve]  );

        cubeElem->setAttribute("avpGain",   cube->guideCurveGains[Vertex::Time]   );
        cubeElem->setAttribute("keyGain",   cube->guideCurveGains[Vertex::Red]    );
        cubeElem->setAttribute("modGain",   cube->guideCurveGains[Vertex::Blue]   );
        cubeElem->setAttribute("phaseGain", cube->guideCurveGains[Vertex::Phase]  );
        cubeElem->setAttribute("ampGain",   cube->guideCurveGains[Vertex::Amp]    );
        cubeElem->setAttribute("curveGain", cube->guideCurveGains[Vertex::Curve]  );

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
    vector<VertCube*> failedLines;

    auto readCube = [this, &idMap, &failedVerts, &failedLines](const XmlElement* currentCube) -> void {
        auto* cube = new VertCube();

        cube->guideCurveGains[Vertex::Amp]   = currentCube->getDoubleAttribute("ampGain",   0.5);
        cube->guideCurveGains[Vertex::Blue]  = currentCube->getDoubleAttribute("modGain",   0.5);
        cube->guideCurveGains[Vertex::Curve] = currentCube->getDoubleAttribute("curveGain", 0.5);
        cube->guideCurveGains[Vertex::Phase] = currentCube->getDoubleAttribute("phaseGain", 0.5);
        cube->guideCurveGains[Vertex::Red]   = currentCube->getDoubleAttribute("keyGain",   0.5);
        cube->guideCurveGains[Vertex::Time]  = currentCube->getDoubleAttribute("avpGain",   0.5);

        cube->guideCurveChans[Vertex::Amp]   = currentCube->getIntAttribute("ampGuide",   -1);
        cube->guideCurveChans[Vertex::Blue]  = currentCube->getIntAttribute("modGuide",   -1);
        cube->guideCurveChans[Vertex::Curve] = currentCube->getIntAttribute("curveGuide", -1);
        cube->guideCurveChans[Vertex::Phase] = currentCube->getIntAttribute("phaseGuide", -1);
        cube->guideCurveChans[Vertex::Red]   = currentCube->getIntAttribute("keyGuide",   -1);
        cube->guideCurveChans[Vertex::Time]  = currentCube->getIntAttribute("avpGuide",   -1);

        if(cube->getCompGuideCurve() < 0) {
            cube->getCompGuideCurve() = currentCube->getIntAttribute("timeGuide",  -1);
        }

        int numVertsSet = 0;
        bool failed = false;

        for(auto currentVert : currentCube->getChildWithTagNameIterator("Vertex")) {
            if (currentVert == nullptr) {
                jassertfalse;
                failed = true;
                break;
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
    };

    for(auto currentCube : meshElem->getChildWithTagNameIterator("VertCube")) {
        readCube(currentCube);
    }

    for(auto currentCube : meshElem->getChildWithTagNameIterator("LineCube")) {
        readCube(currentCube);
    }

    validate();

    return true;
}

var Mesh::writeJSON() const {
    auto json = PresetJson::object();
    Array<var> vertexArray, cubeArray;

    json->setProperty("name", name);
    json->setProperty("version", version);

    int count = 0;
    map<Vertex*, int> idMap;

    for (auto* vert : verts) {
        auto vertex = PresetJson::object();

        vertex->setProperty("time",   vert->values[Vertex::Time]);
        vertex->setProperty("phase",  vert->values[Vertex::Phase]);
        vertex->setProperty("amp",    vert->values[Vertex::Amp]);
        vertex->setProperty("key",    vert->values[Vertex::Red]);
        vertex->setProperty("mod",    vert->values[Vertex::Blue]);
        vertex->setProperty("weight", vert->values[Vertex::Curve]);
        vertex->setProperty("id", count);

        idMap[vert] = count++;
        vertexArray.add(PresetJson::toVar(vertex));
    }

    for (auto* cube : cubes) {
        auto cubeJson = PresetJson::object();
        auto guides = PresetJson::object();
        auto gains = PresetJson::object();
        Array<var> vertexIds;

        for (int i = 0; i < VertCube::numVerts; ++i) {
            vertexIds.add(idMap[cube->getVertex(i)]);
        }

        guides->setProperty("time",  cube->guideCurveChans[Vertex::Time]);
        guides->setProperty("key",   cube->guideCurveChans[Vertex::Red]);
        guides->setProperty("mod",   cube->guideCurveChans[Vertex::Blue]);
        guides->setProperty("phase", cube->guideCurveChans[Vertex::Phase]);
        guides->setProperty("amp",   cube->guideCurveChans[Vertex::Amp]);
        guides->setProperty("curve", cube->guideCurveChans[Vertex::Curve]);

        gains->setProperty("time",  cube->guideCurveGains[Vertex::Time]);
        gains->setProperty("key",   cube->guideCurveGains[Vertex::Red]);
        gains->setProperty("mod",   cube->guideCurveGains[Vertex::Blue]);
        gains->setProperty("phase", cube->guideCurveGains[Vertex::Phase]);
        gains->setProperty("amp",   cube->guideCurveGains[Vertex::Amp]);
        gains->setProperty("curve", cube->guideCurveGains[Vertex::Curve]);

        cubeJson->setProperty("vertexIds", var(vertexIds));
        cubeJson->setProperty("guides", PresetJson::toVar(guides));
        cubeJson->setProperty("gains", PresetJson::toVar(gains));
        cubeArray.add(PresetJson::toVar(cubeJson));
    }

    json->setProperty("vertices", var(vertexArray));
    json->setProperty("cubes", var(cubeArray));

    return PresetJson::toVar(json);
}

bool Mesh::readJSON(const var& object) {
    const Array<var>* vertexArray = PresetJson::getArray(PresetJson::property(object, "vertices"));
    const Array<var>* cubeArray = PresetJson::getArray(PresetJson::property(object, "cubes"));

    if (PresetJson::getObject(object) == nullptr || vertexArray == nullptr || cubeArray == nullptr) {
        return false;
    }

    destroy();

    version = PresetJson::intProperty(object, "version", Constants::MeshFormatVersion);
    name = PresetJson::stringProperty(object, "name", "unnamed");

    DBG(String::formatted("Mesh::readJSON name=%s version=%d vertices=%d cubes=%d",
                          name.toRawUTF8(),
                          version,
                          vertexArray != nullptr ? vertexArray->size() : -1,
                          cubeArray != nullptr ? cubeArray->size() : -1));

    map<int, Vertex*> idMap;

    for (const auto& vertexValue : *vertexArray) {
        auto* vert = new Vertex();

        vert->values[Vertex::Time]  = PresetJson::doubleProperty(vertexValue, "time");
        vert->values[Vertex::Phase] = PresetJson::doubleProperty(vertexValue, "phase");
        vert->values[Vertex::Amp]   = PresetJson::doubleProperty(vertexValue, "amp");
        vert->values[Vertex::Red]   = PresetJson::doubleProperty(vertexValue, "key");
        vert->values[Vertex::Blue]  = PresetJson::doubleProperty(vertexValue, "mod");
        vert->values[Vertex::Curve] = PresetJson::doubleProperty(vertexValue, "weight");

        int id = PresetJson::intProperty(vertexValue, "id", (int) verts.size());

        idMap[id] = vert;
        verts.push_back(vert);
    }

    int skippedWrongVertexCount = 0;
    int skippedInvalidMapping = 0;

    if (cubeArray != nullptr) {
        for (const auto& cubeValue : *cubeArray) {
            const Array<var>* vertexIds = PresetJson::getArray(PresetJson::property(cubeValue, "vertexIds"));
            auto guides = PresetJson::property(cubeValue, "guides");
            auto gains = PresetJson::property(cubeValue, "gains");

            if (vertexIds == nullptr || vertexIds->size() != VertCube::numVerts) {
                ++skippedWrongVertexCount;
                DBG(String::formatted("Mesh::readJSON skipping cube in %s: vertexIds size=%d expected=%d",
                                      name.toRawUTF8(),
                                      vertexIds != nullptr ? vertexIds->size() : -1,
                                      VertCube::numVerts));
                continue;
            }

            auto* cube = new VertCube();

            cube->guideCurveChans[Vertex::Time]  = PresetJson::intProperty(guides, "time", -1);
            cube->guideCurveChans[Vertex::Red]   = PresetJson::intProperty(guides, "key", -1);
            cube->guideCurveChans[Vertex::Blue]  = PresetJson::intProperty(guides, "mod", -1);
            cube->guideCurveChans[Vertex::Phase] = PresetJson::intProperty(guides, "phase", -1);
            cube->guideCurveChans[Vertex::Amp]   = PresetJson::intProperty(guides, "amp", -1);
            cube->guideCurveChans[Vertex::Curve] = PresetJson::intProperty(guides, "curve", -1);

            cube->guideCurveGains[Vertex::Time]  = PresetJson::doubleProperty(gains, "time", 0.5);
            cube->guideCurveGains[Vertex::Red]   = PresetJson::doubleProperty(gains, "key", 0.5);
            cube->guideCurveGains[Vertex::Blue]  = PresetJson::doubleProperty(gains, "mod", 0.5);
            cube->guideCurveGains[Vertex::Phase] = PresetJson::doubleProperty(gains, "phase", 0.5);
            cube->guideCurveGains[Vertex::Amp]   = PresetJson::doubleProperty(gains, "amp", 0.5);
            cube->guideCurveGains[Vertex::Curve] = PresetJson::doubleProperty(gains, "curve", 0.5);

            bool valid = true;

            for (int i = 0; i < VertCube::numVerts; ++i) {
                int vertexId = int(vertexIds->getReference(i));
                auto it = idMap.find(vertexId);

                if (it == idMap.end() || !cube->setVertex(it->second, i)) {
                    DBG(String::formatted("Mesh::readJSON invalid cube mapping in %s: cubeVertex=%d vertexId=%d found=%d",
                                          name.toRawUTF8(),
                                          i,
                                          vertexId,
                                          it != idMap.end() ? 1 : 0));
                    valid = false;
                    break;
                }
            }

            if (valid) {
                cubes.push_back(cube);
            } else {
                ++skippedInvalidMapping;
                delete cube;
            }
        }
    } else {
        DBG("cubeArray is empty");
    }

    DBG(String::formatted("Mesh::readJSON complete name=%s verts=%d cubes=%d skippedWrongVertexCount=%d skippedInvalidMapping=%d",
                          name.toRawUTF8(),
                          (int) verts.size(),
                          (int) cubes.size(),
                          skippedWrongVertexCount,
                          skippedInvalidMapping));

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
