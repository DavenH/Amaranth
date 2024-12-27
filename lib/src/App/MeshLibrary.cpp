#include "MeshLibrary.h"

#include <Definitions.h>

#include "EditWatcher.h"
#include "SingletonRepo.h"
#include "../Curve/Mesh.h"
#include "../Curve/EnvelopeMesh.h"
#include "../Util/CommonEnums.h"
#include "../UI/IConsole.h"

MeshLibrary::MeshLibrary(SingletonRepo* repo) :
        SingletonAccessor(repo, "MeshLibrary")
    ,   clipboardMesh()
    ,   dummyGroup(TypeMesh)
    ,   dummyLayer() {
}

MeshLibrary::~MeshLibrary() {
    destroy();
}

void MeshLibrary::destroyLayer(Layer& layer) {
    if (layer.mesh != nullptr) {
        layer.mesh->destroy();
        delete layer.mesh;

        layer.mesh = nullptr;
    }

    if (layer.props != nullptr) {
        delete layer.props;
        layer.props = nullptr;
    }

    getObj(EditWatcher).setHaveEditedWithoutUndo(true);
}

void MeshLibrary::destroy() {
    ScopedLock sl(arrayLock);

    for (auto& layerGroup : layerGroups) {
        for (auto& layer : layerGroup.layers) {
            destroyLayer(layer);
        }
    }

    layerGroups.clear();
}

void MeshLibrary::addGroup(int meshType) {
    ScopedLock sl(arrayLock);

    LayerGroup group(meshType);
    layerGroups.push_back(group);

    getObj(EditWatcher).setHaveEditedWithoutUndo(true);
}

void MeshLibrary::moveLayer(int groupId, int fromIndex, int toIndex) {
    if (!isPositiveAndBelow(groupId, (int) layerGroups.size()) || fromIndex == toIndex) {
        return;
    }

    LayerGroup& group = layerGroups[groupId];
    std::swap(group.layers[fromIndex], group.layers[toIndex]);

    listeners.call(&Listener::layerChanged, groupId, toIndex);
    // todo does that work?
    // if not, can try
    // iter_swap(group.layers.begin() + currentIndex, group.layers.begin() + currentIndex + movement);
}

MeshLibrary::LayerGroup& MeshLibrary::getLayerGroup(int group) {
    if (group == CommonEnums::Null || group >= layerGroups.size()) {
        return dummyGroup;
    }

    return layerGroups[group];
}

void MeshLibrary::addLayer(int groupId) {
    LayerGroup& group = getLayerGroup(groupId);

    ScopedLock sl(arrayLock);
    group.layers.push_back(instantiateLayer(nullptr, group.meshType));

    int index = group.layers.size() - 1;
    listeners.call(&Listener::layerAdded, groupId, index);
}

void MeshLibrary::setCurrentMesh(int groupId, Mesh* mesh) {
    ScopedLock sl(arrayLock);

    getCurrentLayer(groupId).mesh = mesh;
}

MeshLibrary::Layer& MeshLibrary::getCurrentLayer(int groupId) {
    LayerGroup& group = getLayerGroup(groupId);

    if (isPositiveAndBelow(group.current, (int) group.layers.size())) {
        return group.layers[group.current];
    }

    return dummyLayer;
}

void MeshLibrary::copyToClipboard(Mesh* mesh, int type) {
    EnvelopeMesh* envMesh;
    if ((envMesh = dynamic_cast<EnvelopeMesh*>(mesh)) != nullptr) {
        clipboardMesh.envMesh->deepCopy(envMesh);
    } else {
        clipboardMesh.mesh->deepCopy(mesh);
    }

    clipboardMesh.type = type;
}

bool MeshLibrary::canPasteTo(int type) const {
    return type == clipboardMesh.type;
}

void MeshLibrary::pasteFromClipboardTo(Mesh* mesh, int type) {
    Mesh* sourceClipboardMesh = (type == TypeEnvelope) ? (Mesh*) clipboardMesh.envMesh.get()
                                                       : clipboardMesh.mesh.get();

    if (sourceClipboardMesh->getNumVerts() == 0) {
        showConsoleMsg("Clipboard's mesh is empty.");
        return;
    }

    if (canPasteTo(type)) {
        mesh->deepCopy(sourceClipboardMesh);

        // anything to change about deformers here?
    } else {
        showConsoleMsg("Cannot paste between incompatible mesh types");
    }
}

bool MeshLibrary::removeLayerKeepingOne(int groupId, int layer) {
    ScopedLock sl(arrayLock);

    LayerGroup& group = getLayerGroup(groupId);

    if (isPositiveAndBelow(layer, (int) group.layers.size())) {
        destroyLayer(group.layers[layer]);
    }

    if (group.layers.size() == 1) {
        group.layers.front() = instantiateLayer(nullptr, group.meshType);
        return true;
    }

    group.layers.erase(group.layers.begin() + layer);

    return false;
}

MeshLibrary::Layer MeshLibrary::instantiateLayer(XmlElement* layerElem, int meshType) {
    Layer layer{};

    switch (meshType) {
        case TypeMesh:
            layer.mesh = new Mesh();
            layer.props = new Properties();
            break;

        case TypeMesh2D:
            layer.mesh = new Mesh();
            layer.props = nullptr;
            break;

        case TypeEnvelope:
            layer.mesh = new EnvelopeMesh(String());
            layer.props = new EnvProps();
            break;

        default: {
            throw std::runtime_error("Unsupported mesh type");
        }
    }

    if (layerElem != nullptr) {
        layer.mesh->readXML(layerElem);

        if (layer.props != nullptr) {
            layer.props->readXML(layerElem);
        }
    }

    return layer;
}

void MeshLibrary::writeXML(XmlElement* element) const {
    auto* repoElem = new XmlElement("MeshLibrary");

    ScopedLock sl(arrayLock);

    for (const auto& layerGroup : layerGroups) {
        auto* groupElem = new XmlElement("Group");
        groupElem->setAttribute("mesh-type", layerGroup.meshType);

        for (auto layer : layerGroup.layers) {
            auto* layerElem = new XmlElement("Layer");

            layer.mesh->writeXML(layerElem);

            if (layer.props != nullptr) {
                layer.props->writeXML(layerElem);
            }

            groupElem->addChildElement(layerElem);
        }

        repoElem->addChildElement(groupElem);
    }

    element->addChildElement(repoElem);
}

bool MeshLibrary::readXML(const XmlElement* element) {
    XmlElement* repoElem = element->getChildByName("MeshLibrary");

    if (repoElem == nullptr) {
        return false;
    }

    ScopedLock sl(arrayLock);

    destroy();
    for(auto& layerGroup : layerGroups) {
        listeners.call(&Listener::layerGroupDeleted, layerGroup.meshType);
    }
    layerGroups.clear();

    for(auto groupElem : repoElem->getChildWithTagNameIterator("Group")) {
        LayerGroup group(groupElem->getIntAttribute("mesh-type", TypeMesh));

        for(auto layerElem : groupElem->getChildWithTagNameIterator("Layer")) {
            Layer layer = instantiateLayer(layerElem, group.meshType);
            group.layers.push_back(layer);
        }

        layerGroups.push_back(group);
    }
    for(auto& layerGroup : layerGroups) {
        listeners.call(&Listener::layerGroupAdded, layerGroup.meshType);
    }
    return true;
}

MeshLibrary::Properties::Properties() {
    smoothedParameters.emplace_back(&pan);
    smoothedParameters.emplace_back(&fineTune);

    for(auto& p : pos) {
        smoothedParameters.emplace_back(&p.time);
        smoothedParameters.emplace_back(&p.red);
        smoothedParameters.emplace_back(&p.blue);
    }
}

bool MeshLibrary::Properties::readXML(const XmlElement* layerElem) {
    active      = layerElem->getBoolAttribute("active", active);
    gain        = layerElem->getDoubleAttribute("gain", gain);
    mode        = layerElem->getIntAttribute("mode", mode);
    range       = layerElem->getDoubleAttribute("range", range);
    scratchChan = layerElem->getIntAttribute("scratch-chan", scratchChan);

    fineTune.setTargetValue(layerElem->getDoubleAttribute("fine-tune", fineTune.getTargetValue()));
    pan.setTargetValue(layerElem->getDoubleAttribute("pan", pan.getTargetValue()));

    return true;
}

bool MeshLibrary::EnvProps::readXML(const XmlElement* layerElem) {
    Properties::readXML(layerElem);

    dynamic     = layerElem->getBoolAttribute("dynamic", dynamic);
    global      = layerElem->getBoolAttribute("global", global);
    logarithmic = layerElem->getBoolAttribute("logarithmic", logarithmic);
    scale       = layerElem->getIntAttribute("scale", scale);
    tempoSync   = layerElem->getBoolAttribute("tempo-sync", tempoSync);

    return true;
}

void MeshLibrary::Properties::writeXML(XmlElement* layerElem) const {
    layerElem->setAttribute("active", active);
    layerElem->setAttribute("gain", gain);
    layerElem->setAttribute("range", range);
    layerElem->setAttribute("mode", mode);
    layerElem->setAttribute("scratch-chan", scratchChan);

    layerElem->setAttribute("fine-tune", fineTune.getTargetValue());
    layerElem->setAttribute("pan", pan.getTargetValue());
}

void MeshLibrary::Properties::setDimValue(int index, int dim, float value) {
    if(dim == Vertex::Time) {
        pos[index][dim].setValueDirect(value);
    } else {
        pos[index][dim].setTargetValue(value);
    }
}

void MeshLibrary::Properties::updateSmoothedParameters(int sampleCount44k) {
    for(auto* p : smoothedParameters) {
        p->update(sampleCount44k);
    }
}

void MeshLibrary::Properties::updateToTarget(int voiceIndex) {
    pos[voiceIndex].time.updateToTarget();
    pos[voiceIndex].red.updateToTarget();
    pos[voiceIndex].blue.updateToTarget();
}

void MeshLibrary::Properties::updateParameterSmoothing(bool smooth) {
    for(auto* p : smoothedParameters) {
        p->setSmoothingActivity(smooth);
    }
    if(smooth) {
        for(auto* p : smoothedParameters) {
            p->updateToTarget();
        }
    }
}

void MeshLibrary::EnvProps::writeXML(XmlElement* layerElem) const {
    Properties::writeXML(layerElem);

    layerElem->setAttribute("dynamic", dynamic);
    layerElem->setAttribute("tempo-sync", tempoSync);
    layerElem->setAttribute("global", global);
    layerElem->setAttribute("logarithmic", logarithmic);
    layerElem->setAttribute("scale", scale);
}

bool MeshLibrary::hasAnyValidLayers(int groupId) {
    LayerGroup& group = getLayerGroup(groupId);

    bool haveAnyValidTimeLayers = false;

    for (int i = 0; i < group.size(); ++i) {
        Layer layer = getLayer(groupId, i);

        if (layer.props->active && layer.mesh->hasEnoughCubesForCrossSection()) {
            haveAnyValidTimeLayers = true;
            break;
        }
    }

    return haveAnyValidTimeLayers;
}

MeshLibrary::EnvProps* MeshLibrary::getEnvProps(int group, int index) {
    return dynamic_cast<EnvProps*>(getProps(group, index));
}

EnvelopeMesh* MeshLibrary::getEnvMesh(int group, int index) {
    return dynamic_cast<EnvelopeMesh*>(getMesh(group, index));
}

EnvelopeMesh* MeshLibrary::getCurrentEnvMesh(int group) {
    return dynamic_cast<EnvelopeMesh*>(getCurrentMesh(group));
}

bool MeshLibrary::layerRemoved(int layerGroup, int index) {
    bool changed = false;

    switch (layerGroup) {
        case LayerGroups::GroupDeformer: {
            for (int i = 0; i < getNumGroups(); ++i) {
                LayerGroup& group = layerGroups[i];

                for (int j = 0; j < group.size(); ++j) {
                    Layer& layer = group.layers[j];

                    for(auto& cube : layer.mesh->getCubes()) {
                        for (int k = Vertex::Time; k <= Vertex::Curve; ++k) {
                            char& chan = cube->deformerAt(k);

                            if (chan > index) {
                                --chan;
                                changed = true;
                            } else if (chan == index) {
                                chan = -1;
                                changed = true;
                            }
                        }
                    }
                }
            }

            break;
        }

        case LayerGroups::GroupScratch:
            break;

        default:
            break;
    }

    listeners.call(&Listener::layerRemoved, layerGroup, index);

    return changed;
}

bool MeshLibrary::layerChanged(int layerGroup, int index) {
    LayerGroup& srcGroup = layerGroups[layerGroup];

    switch (layerGroup) {
        case LayerGroups::GroupScratch: {
            int types[] = {
                LayerGroups::GroupTime,
                LayerGroups::GroupSpect,
                LayerGroups::GroupPhase
            };

            srcGroup.sources.clear();

            for (int i = 0; i < numElementsInArray(types); ++i) {
                LayerGroup& group = layerGroups[i];

                for (int j = 0; j < group.size(); ++j) {
                    int chan = group[j].props->scratchChan;

                    if (chan != CommonEnums::Null) {
                        srcGroup.sources[chan].add(types[i]);
                    }
                }
            }

            break;
        }

        case LayerGroups::GroupDeformer: {

            int types[] = {
                LayerGroups::GroupTime,
                LayerGroups::GroupSpect,
                LayerGroups::GroupPhase,
                LayerGroups::GroupVolume,
                LayerGroups::GroupPitch,
                LayerGroups::GroupScratch
            };

            srcGroup.sources.clear();

            for (int i = 0; i < numElementsInArray(types); ++i) {
                if (i == LayerGroups::GroupDeformer) {
                    continue;
                }

                LayerGroup& group = layerGroups[i];

                for (int j = 0; j < group.size(); ++j) {
                    for(auto& cube : group[j].mesh->getCubes()) {
                        for (int d = 0; d <= Vertex::Curve; ++d) {
                            char& deformChan = cube->deformerAt(d);

                            if (deformChan >= 0) {
                                group.sources[deformChan].add(types[i]);
                            }
                        }
                    }
                }
            }

            break;
        }
        default: break;
    }

    listeners.call(&Listener::layerChanged, layerGroup, index);

    return false;
}

Array<int> MeshLibrary::getMeshTypesAffectedByCurrent(int layerGroup) {
    LayerGroup& group = layerGroups[layerGroup];

    return group.sources[group.current];
}

void MeshLibrary::updateSmoothedParameters(int voiceIndex, int numSamples44k) const {
    for(auto& group : layerGroups) {
        for(auto& layer : group.layers) {
            if(layer.props) {
                layer.props->pos[voiceIndex].update(numSamples44k);
            }
        }
    }
}

void MeshLibrary::updateAllSmoothedParamsToTarget(int voiceIndex) const {
    for(auto& group : layerGroups) {
        for(auto& layer : group.layers) {
            if(layer.props) {
                layer.props->pos[voiceIndex].updateToTarget();
            }
        }
    }
}
