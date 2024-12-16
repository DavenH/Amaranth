#pragma once

#include <map>
#include <vector>
#include "SingletonAccessor.h"
#include "Doc/Savable.h"
#include "../Obj/MorphPosition.h"
#include "../Util/CommonEnums.h"

using std::vector;
using std::map;

class Mesh;
class EnvelopeMesh;
class Vertex;

namespace LayerGroups {
    enum {
		GroupVolume
	,	GroupPitch
	,	GroupScratch
	,	GroupDeformer
	,	GroupTime
	,	GroupSpect
	,	GroupPhase
	,	numDefaultGroups
	};
}
class MeshLibrary :
		public SingletonAccessor
	,	public Savable {
public:
	enum {
		TypeMesh
	,	TypeMesh2D
	,	TypeEnvelope
	};

    /* ----------------------------------------------------------------------------- */

	struct Properties : Savable {
		~Properties() override = default;
		bool readXML(const XmlElement* element) override;
		void writeXML(XmlElement* element) const override;

        void setDimValue(int index, int dim, float value) {
			pos[index][dim] = value;
		}

		bool active;
		int  scratchChan, mode;
		float gain, pan, fineTune, range;

		MorphPosition pos[12];
	};

    struct EnvProps : public Properties {
		bool readXML(const XmlElement* element) override;
		void writeXML(XmlElement* element) const override;

		float getEffectiveScale() const { return scale < 0 ? 1.f / -scale : scale; }
		bool isOperating() const		{ return dynamic || global || scale != 1 || tempoSync || logarithmic; }

		bool  dynamic, tempoSync, global, logarithmic;
		int   scale;
		float tempoScale;
	};

    struct Layer {
		Mesh* mesh;
		Properties* props;
	};

    struct LayerGroup {
		explicit LayerGroup(int type) : meshType(type), current(0) {}
		int size() const { return layers.size(); }
		Layer& operator[] (const int idx) { return layers[idx]; }

		Mesh* getCurrentMesh()
		{
			if(current < 0)
				return nullptr;

			return layers[current].mesh;
		}

		int current;
		int meshType;

		map<int, Array<int> > sources;
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


    class Listener {
	public:
	    virtual ~Listener() = default;

	    virtual void layerAdded(int layerGroup, int index) {}
		virtual void layerRemoved(int layerGroup, int index) {}
		virtual void layerChanged(int layerGroup, int index) {}
		virtual void allLayersDeleted() {}
		virtual void instantiateLayer(XmlElement* layerElem, int meshType) {}
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

	bool canPasteTo(int type);
	void copyToClipboard(Mesh* mesh, int type);
	void pasteFromClipboardTo(Mesh* mesh, int type);


	bool layerRemoved(int layerGroup, int index);
	bool layerChanged(int layerGroup, int index);
	Array<int> getMeshTypesAffectedByCurrent(int layerGroup);

	Layer instantiateLayer(XmlElement* meshElem, int meshType);

	Layer& 			getCurrentLayer(int group);
	LayerGroup& 	getGroup(int group);

	EnvProps* 		getEnvProps(const GroupLayerPair& pair) { return getEnvProps(pair.groupId, pair.layerIdx); }
	EnvelopeMesh* 	getEnvMesh(const GroupLayerPair& pair) { return getEnvMesh(pair.groupId, pair.layerIdx); }
	EnvProps* 		getEnvProps(int group, int index);
	EnvelopeMesh* 	getEnvMesh(int group, int index);

	int 			getNumGroups() 					{ return (int) layerGroups.size(); 		}
	int				getCurrentIndex(int group) 		{ return getGroup(group).current; 		}
	Mesh* 			getMesh(int group, int index) 	{ return getLayer(group, index).mesh;   }
	Mesh* 			getCurrentMesh(int group) 		{ return getCurrentLayer(group).mesh; 	}
	Layer& 			getLayer(int group, int index) 	{ return getGroup(group).layers[index]; }
	Properties* 	getProps(int group, int index) 	{ return getLayer(group, index).props;  }
	Properties* 	getCurrentProps(int group) 		{ return getCurrentLayer(group).props; 	}
	EnvProps* 		getCurrentEnvProps(int group) 	{ return dynamic_cast<EnvProps*>(getCurrentLayer(group).props); }
	EnvelopeMesh* 	getCurrentEnvMesh(int group);

	void setCurrentMesh(int groupId, Mesh* mesh);

	CriticalSection& getLock() 						{ return arrayLock; 				 	}
	vector<Vertex*>& getSelectedByType(int type) 	{ return layerGroups[type].selected; 	}

	void addListener(Listener* listener) 			{ listeners.add(listener); }

protected:
	CriticalSection arrayLock;
	ClipboardMesh clipboardMesh;

	Layer dummyLayer;
	LayerGroup dummyGroup;
	vector<LayerGroup> layerGroups;
	ListenerList<Listener> listeners;

	typedef vector<LayerGroup>::iterator GroupIter;
	typedef vector<Layer>::iterator LayerIter;
};