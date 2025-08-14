#pragma once

#include <map>
#include <vector>
#include "SingletonAccessor.h"
#include "Doc/Savable.h"
#include "../Wireframe/Interpolator/Trilinear/MorphPosition.h"
#include "../Util/CommonEnums.h"

using std::vector;
using std::map;

class Mesh;
class EnvelopeMesh;
class Vertex;

namespace LayerGroups {
    enum {
        GroupVolume
    ,   GroupPitch
    ,   GroupScratch
    ,   GroupPath
    ,   GroupTime
    ,   GroupSpect
    ,   GroupPhase
    ,   numDefaultGroups
    };
}
class MeshLibrary :
        public SingletonAccessor
    ,   public Savable {
public:
    enum {
        TypeMesh
    ,   TypeMesh2D
    ,   TypeEnvelope
    };

    /* ----------------------------------------------------------------------------- */

    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void layerGroupAdded(int layerGroup) {}
        virtual void layerGroupDeleted(int layerGroup) {}
        virtual void layerAdded(int layerGroup, int index) {}
        virtual void layerRemoved(int layerGroup, int index) {}
        virtual void layerChanged(int layerGroup, int index) {}
        virtual void instantiateLayer(XmlElement* layerElem, int meshType) {}
    };

    /* ----------------------------------------------------------------------------- */

    struct Properties : Savable {
        Properties();
        ~Properties() override = default;
        bool readXML(const XmlElement* element) override;
        void writeXML(XmlElement* element) const override;

        // this is the result of mod matrix value automation
        void setDimValue(int index, int dim, float value);
        virtual void updateSmoothedParameters(int sampleCount44k);
        void updateParameterSmoothing(bool smooth);
        void updateToTarget(int voiceIndex);

        bool active{};
        int  scratchChan{}, mode{};
        float gain{}, range{};

        MorphPosition pos[12];
        SmoothedParameter pan, fineTune;
        vector<SmoothedParameter*> smoothedParameters;
    };

    struct EnvProps : Properties {
        bool readXML(const XmlElement* element) override;
        void writeXML(XmlElement* element) const override;

        [[nodiscard]] float getEffectiveScale() const { return scale < 0 ? 1.f / -scale : scale; }
        [[nodiscard]] bool isOperating() const { return dynamic || global || scale != 1 || tempoSync || logarithmic; }

        bool  dynamic, tempoSync, global, logarithmic;
        int   scale;
        float tempoScale;
    };

    struct Layer {
        Mesh* mesh;
        Properties* props;
    };

    struct LayerGroup {
        explicit LayerGroup(int type)
            : meshType(type),
              current(0) {
        }

        [[nodiscard]] int size() const { return layers.size(); }
        Layer& operator[](const int idx) { return layers[idx]; }

        [[nodiscard]] Mesh* getCurrentMesh() const {
            if (current < 0) {
                return nullptr;
            }

            return layers[current].mesh;
        }

        int current;
        int meshType;

        map<int, Array<int>> sources;
        vector<Layer> layers;
        vector<Vertex*> selected;
    };

    struct ClipboardMesh {
        int type;
        std::unique_ptr<Mesh> mesh;
        std::unique_ptr<EnvelopeMesh> envMesh;
    };

    struct GroupLayerPair {
        int groupId, layerIdx;
        GroupLayerPair(int id, int idx) : groupId(id), layerIdx(idx) {}
        [[nodiscard]] bool isNotNull() const { return groupId != CommonEnums::Null && layerIdx != CommonEnums::Null; }
    };

    /* ----------------------------------------------------------------------------- */

    explicit MeshLibrary(SingletonRepo* repo);
    ~MeshLibrary() override;

    bool readXML(const XmlElement* element) override;
    void writeXML(XmlElement* element) const override;

    void addGroup(int meshType);
    void addLayer(int group);
    void destroy();
    void destroyLayer(Layer& layer);
    void moveLayer(int layerId, int fromIndex, int toIndex);
    bool removeLayerKeepingOne(int group, int layer);
    bool hasAnyValidLayers(int groupId);
    bool canPasteTo(int type) const;
    void copyToClipboard(Mesh* mesh, int type);
    void pasteFromClipboardTo(Mesh* mesh, int type);
    bool layerRemoved(int layerGroup, int index);
    bool layerChanged(int layerGroup, int index);

    Array<int> getMeshTypesAffectedByCurrent(int layerGroup);
    Layer instantiateLayer(XmlElement* meshElem, int meshType);
    void setCurrentMesh(int groupId, Mesh* mesh);

    [[nodiscard]] Layer&        getCurrentLayer(int layerGroup);
    [[nodiscard]] LayerGroup&   getLayerGroup(int layerGroup);

    [[nodiscard]] EnvProps*     getEnvProps(const GroupLayerPair& pair) { return getEnvProps(pair.groupId, pair.layerIdx); }
    [[nodiscard]] EnvelopeMesh* getEnvMesh(const GroupLayerPair& pair) { return getEnvMesh(pair.groupId, pair.layerIdx); }
    [[nodiscard]] EnvProps*     getEnvProps(int group, int index);
    [[nodiscard]] EnvelopeMesh* getEnvMesh(int group, int index);

    [[nodiscard]] int           getNumGroups()                  { return (int) layerGroups.size();      }
    [[nodiscard]] int           getCurrentIndex(int group)      { return getLayerGroup(group).current;  }
    [[nodiscard]] Mesh*         getMesh(int group, int index)   { return getLayer(group, index).mesh;   }
    [[nodiscard]] Mesh*         getCurrentMesh(int group)       { return getCurrentLayer(group).mesh;   }
    [[nodiscard]] Layer&        getLayer(int group, int index)  { return getLayerGroup(group).layers[index]; }
    [[nodiscard]] Properties*   getProps(int group, int index)  { return getLayer(group, index).props;  }
    [[nodiscard]] Properties*   getCurrentProps(int group)      { return getCurrentLayer(group).props;  }
    [[nodiscard]] EnvProps*     getCurrentEnvProps(int group)   { return dynamic_cast<EnvProps*>(getCurrentLayer(group).props); }
    [[nodiscard]] EnvelopeMesh* getCurrentEnvMesh(int group);

    [[nodiscard]] CriticalSection& getLock()                    { return arrayLock;                     }
    [[nodiscard]] vector<Vertex*>& getSelectedByType(int type)  { return layerGroups[type].selected;    }

    void addListener(Listener* listener) { listeners.add(listener); }
    void updateSmoothedParameters(int voiceIndex, int numSamples44k) const;
    void updateAllSmoothedParamsToTarget(int voiceIndex) const;

protected:
    CriticalSection arrayLock;
    ClipboardMesh clipboardMesh;

    Layer dummyLayer;
    LayerGroup dummyGroup;
    vector<LayerGroup> layerGroups;
    ListenerList<Listener> listeners;
};