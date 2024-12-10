#pragma once

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updater.h>

class EnvelopeDelegate;
class SpectDelegate;
class MorphUpdate;
class ScratchUpdate;

class CycleUpdater :
		public SingletonAccessor
	,	public MeshLibrary::Listener {
public:
	typedef Updater::Node Node;

	explicit CycleUpdater(SingletonRepo* repo);
	~CycleUpdater() override = default;

	void init() override;
	void createUpdateGraph();
	void envelopeVisibilityChanged();
	void moveTimeUIs(int viewStage, int lastViewStage);
	void viewStageChanged(bool force);
	void setDspFXConnections();
	void setTimeFreqChildren(bool toFFT);
	void setTimeFreqParents();
	void removeTimeFreqChildren(bool fromFFT);
	void removeTimeFreqParents();
	void removeDspFXConnections();
	void refreshConnections(Node* destNode, const Array<int>& meshTypes);
	void layerChanged(int layerGroup, int index) override;

private:
	Node* time2Itr;
	Node* time3Itr;
	Node* spect2Itr;
	Node* spect3Itr;
	Node* env2Itr;
	Node* env3Itr;
	Node* irModelItr;
	Node* wshpItr;
	Node* dfrmItr;
	Node* derivUI;

	Node* time2Rast;
	Node* dfrmRast;
	Node* irModelRast;
	Node* wshpRast;

	Node* envProc;
	Node* timeProc;
	Node* spectProc;
	Node* effectsProc;

	Node* univNode;
	Node* allButFX;

	Node* wshpDsp;
	Node* irModelDsp;

	Node* unisonItr;
	Node* unison;

	Node* ctrlNode;

	Node* envDlg;
	Node* spectDlg;
	Node* scratchRast;
	Node* morphNode;
	Node* synthNode;

	Node* eqlzerUI;
	Node* timeUIs;
	Node* spectUIs;

	Ref<Updater> updater;

	int lastViewStage;

	std::unique_ptr<EnvelopeDelegate> envelopeDelegate;
	std::unique_ptr<SpectDelegate> spectDelegate;
	std::unique_ptr<MorphUpdate> morphUpdate;
	std::unique_ptr<ScratchUpdate> scratchUpdate;
};
