#include <Definitions.h>
#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Curve/VertCube.h>
#include <Design/Updating/Updater.h>
#include <Inter/Interactor.h>
#include <Inter/Interactor3D.h>
#include <Inter/UndoableActions.h>
#include <UI/Panels/Panel.h>
#include <Util/NumberUtils.h>
#include <Util/ScopedBooleanSwitcher.h>
#include <Util/Util.h>

#include "Dialogs.h"
#include "CycleTour.h"
#include "FileManager.h"
#include "KeyboardInputHandler.h"

#include "../Audio/AudioSourceRepo.h"
#include "../Audio/Effects/IrModeller.h"
#include "../Audio/SampleUtils.h"
#include "../Audio/SynthAudioSource.h"
#include "../CycleDefs.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../Inter/WaveformInter2D.h"
#include "../UI/Effects/DelayUI.h"
#include "../UI/Effects/EqualizerUI.h"
#include "../UI/Effects/IrModellerUI.h"
#include "../UI/Effects/ReverbUI.h"
#include "../UI/Effects/UnisonUI.h"
#include "../UI/Effects/WaveshaperUI.h"
#include "../UI/Panels/Console.h"
#include "../UI/Panels/DerivativePanel.h"
#include "../UI/Panels/GeneralControls.h"
#include "../UI/Panels/MainPanel.h"
#include "../UI/Panels/ModMatrixPanel.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/Panels/OscControlPanel.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/Panels/VertexPropertiesPanel.h"
#include "../UI/SynthLookAndFeel.h"
#include "../UI/VertexPanels/DeformerPanel.h"
#include "../UI/VertexPanels/Envelope2D.h"
#include "../UI/VertexPanels/Spectrum2D.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform2D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../Util/CycleEnums.h"

#define A(X) areaStrings.set(#X, X)
#define B(X) subareaStrings.set(#X, X)
#define C(X) condStrings.set(#X, X)
#define P(X) compareStrings.set(#X, X)
#define N(X) actionStrings.set(#X, X)
#define I(X) idStrings.set(#X, X)

CycleTour::CycleTour(SingletonRepo* repo) :
		SingletonAccessor(repo, "CycleTour")
	, 	wrapper			(repo)
	, 	live			(false)
	, 	createdWaveshape(false)
	,	highlighter		(this)
	, 	currentItem		(0)
	, 	lastItem		(0)
	, 	currentTutorial	(0) {
	A(AreaNull), 		A(AreaWshpEditor),	A(AreaSharpBand), 	A(AreaWfrmWaveform3D);
	A(AreaSpectrum),	A(AreaSpectrogram),	A(AreaEnvelopes),	A(AreaVolume);
	A(AreaPitch),		A(AreaScratch),		A(AreaWaveshaper),	A(AreaDeformers);
	A(AreaImpulse),		A(AreaMorphPanel),	A(AreaVertexProps),	A(AreaGenControls);
	A(AreaConsole),		A(AreaPlayback),	A(AreaUnison),		A(AreaReverb);
	A(AreaDelay),		A(AreaEQ),			A(AreaMain),		A(AreaModMatrix);
	A(AreaMasterCtrls);

	B(TargNull), 		B(TargMatrixGrid),	B(TargMatrixUtility), B(TargMatrixSource);
	B(TargMatrixDest), 	B(TargMatrixAddDest),B(TargMatrixAddSource);

	B(TargUniMode),		B(TargUniVoiceSlct), B(TargUniAddRemove);

	B(TargDfrmNoise), 	B(TargDfrmOffset), 	B(TargDfrmPhase), 	B(TargDfrmMeshSlct);
	B(TargDfrmLayerAdd),B(TargDfrmLayerSlct);

	B(TargImpLength), 	B(TargImpGain), 	B(TargImpHP),		B(TargImpZoom);
	B(TargImpLoadWav), 	B(TargImpUnloadWav),B(TargImpModelWav);

	B(TargWaveshaperOvsp), B(TargWaveshaperPre), B(TargWaveshaperPost),	B(TargWaveshaperSlct);

	B(TargSliderArea),	B(TargTimeSlider),	B(TargPhsSlider),	B(TargAmpSlider);
	B(TargKeySlider),	B(TargMorphSlider),	B(TargCrvSlider);

	B(TargBoxArea),		B(TargPhsBox),		B(TargAmpBox),		B(TargKeyBox);
	B(TargModBox), 		B(TargCrvBox),		B(TargAvpBox);

	B(TargGainArea), 	B(TargPhsGain), 	B(TargAmpGain), 	B(TargKeyGain);
	B(TargModGain),		B(TargCrvGain),		B(TargAvpGain);

	B(TargVol),			B(TargScratch),		B(TargPitch), 		B(TargWavPitch);
	B(TargScratchLyr), 	B(TargSustLoop);

	B(TargDomains),		B(TargLayerEnable),	B(TargLayerMode), 	B(TargLayerAdder);
	B(TargLayerMover), 	B(TargLayerSlct), 	B(TargScratchBox),	B(TargDeconv);
	B(TargPhaseUp),		B(TargPan),			B(TargRange),		B(TargMeshSelector);
	B(TargModelCycle);

	B(TargSelector), 	B(TargPencil), 		B(TargAxe),			B(TargNudge);
	B(TargWaveVerts),	B(TargVerts), 		B(TargLinkYellow),	B(TargVertCube);

	B(TargPrimeArea), 	B(TargPrimeY), 		B(TargPrimeB),		B(TargPrimeR);
	B(TargLinkArea), 	B(TargLinkY), 		B(TargLinkB), 		B(TargLinkR);
	B(TargRangeArea), 	B(TargRangeY), 		B(TargRangeB), 		B(TargRangeR);
	B(TargSlidersArea), B(TargSliderY),		B(TargSliderB),		B(TargSliderR);
	B(TargSliderPan);

	B(TargMasterVol), 	B(TargMasterOct),	B(TargMasterLen);

	N(NullAction), 		N(OpenFactoryPreset);
	N(LinkRange),		N(UnlinkRange),		N(TriggerButton),	N(SetVertexSize);
	N(SetUseRange),		N(SetViewStage),	N(ImpLoadWave),		N(ImpModelWave);
	N(ImpUnloadWave),	N(SwitchMode),		N(SwitchToEnv),		N(SwitchToTool);
	N(ShowArea),		N(HideArea),		N(Zoom),			N(DeformLine);
	N(SetNoteOn),		N(SetNoteOff),		N(SetMorphRange),	N(SetLayerMode);
	N(SetLayer),		N(AddPoint),		N(DeletePoint),		N(MovePoint);
	N(SelectPoint),		N(DeselectPoints),	N(SetVertexParam),	N(SetPlaybackPos);
	N(SetDeclick),		N(SetMatrixCell),	N(Enable),			N(Disable);
	N(SetKnobValue),	N(SetPropsValue),	N(LoadRefSample),	N(UnloadSample);
	N(ChangeViewMode),	N(SelectVertAtPos), N(ChopLines), 		N(SetAxeSize);
	N(StartPlayback);

	C(OperNull),		C(OperMoreThanEQ),	C(OperEquals);
	C(OperLessThan),	C(OperLessThanEQ), 	C(OperMoreThan);

	P(NoCompare), 		P(NumPoints), 	 	P(NumLines);
	P(NumLayers), 		P(CurrentLayer), 	P(DfrmAssignments);

	I(IdYellow), 		I(IdRed), 			I(IdBlue);
	I(IdMeshVol), 		I(IdMeshPitch), 	I(IdMeshScratch);
	I(IdToolSelector), 	I(IdToolPencil),	I(IdToolAxe);
	I(IdBttnMoveDown), 	I(IdBttnModeAdditive), I(IdBttnModeFilter);

	I(IdOn), 			I(IdOff),			I(IdZoomIn), 		I(IdZoomOut);
	I(IdModeUniGroup), 	I(IdModeUniSingle),	I(IdViewWave), 		I(IdViewSynth);
	I(IdNull), 			I(IdModeMagn), 		I(IdModePhase);

	I(IdBttnDeconv), 	I(IdBttnPhaseUp),	I(IdBttnLoop), 		I(IdBttnSustain);
	I(IdBttnEnable), 	I(IdBttnAdd), 		I(IdBttnRemove), 	I(IdBttnMoveUp);
	I(IdBttnModel);

	I(IdParamBlue), 	I(IdParamRed), 		I(IdParamSharp);
	I(IdParamTime), 	I(IdParamAvp), 		I(IdParamPhase), 	I(IdParamAmp);
	I(IdViewStageA), 	I(IdViewStageB),	I(IdViewStageC), 	I(IdViewStageD);
}

void CycleTour::init()
{
}

void CycleTour::handleAsyncUpdate()
{
	vector<Item>& items = current.items;

	if(currentItem >= (int) items.size()) {
		exit();
		return;
	}

	Item& item = items[currentItem];
	Item last = items[lastItem];

	if(last.condition.type != NoCompare) {
		if(! conditionPassed(last.condition)) {
			currentItem = lastItem;
			showMsg(last.condition.failMsg);
			return;
		}
	}

	if(! item.actions.empty()) {
		Action& first = item.actions.front();
		performAction(first);

		item.actionIndex = 1;
		startTimer(item.area, first.delayMillis);
	}

//	if(! item.animation.actions.empty() && ! item.animation.hasExecuted)
//	{
//		item.animation.currentIndex = 0;
//		performAction(item.animation.actions.front());
//	}

	Rectangle<int> screenBounds = item.getArea(this);

	if(screenBounds.getWidth() > 0) {
		Point<int> xy;

		if(item.area == AreaModMatrix && last.area != AreaModMatrix) {
			getObj(MainPanel).removeChildComponent(&highlighter);
			getObj(ModMatrix).addAndMakeVisible(&highlighter);
		}
		else if(item.area != AreaModMatrix && last.area == AreaModMatrix) {
			getObj(ModMatrix).removeChildComponent(&highlighter);
			getObj(MainPanel).addAndMakeVisible(&highlighter);
//			wrapper.toFront(true);
		}

		xy = (item.area == AreaModMatrix) ?
			getObj(ModMatrix).getScreenPosition() :
			getObj(MainPanel).getScreenPosition();

		highlighter.update(screenBounds.expanded(4, 4).translated(-xy.getX(), -xy.getY()));

		wrapper.setItem(item);
	}
	else {
		jassertfalse;

		showMsg("Could not find tutorial target");
	}
}

void CycleTour::showNext() {
	if(! current.items.empty()) {
		Item& item = current.items[currentItem];

		// animation not complete
		if(item.actionIndex < item.actions.size()) {
			return;
		}
	}

	lastItem = currentItem;
	++currentItem;
	triggerAsyncUpdate();
}

void CycleTour::showPrevious() {
	if(Util::assignAndWereDifferent(currentItem, jmax(0, currentItem - 1))){
		lastItem = currentItem;
		triggerAsyncUpdate();
	}
}

void CycleTour::enter() {
	getObj(FileManager).openFactoryPreset("empty");

	live = true;
	currentItem = 0;

	wrapper.addToDesktop(ComponentPeer::windowIsTemporary);
	wrapper.setVisible(true);

	getObj(MainPanel).addAndMakeVisible(&highlighter);
	triggerAsyncUpdate();
}

void CycleTour::exit() {
	wrapper.removeFromDesktop();
	wrapper.setVisible(false);

	currentItem = 0;
	lastItem = 0;

	getObj(ModMatrix).removeChildComponent(&highlighter);
	getObj(MainPanel).removeChildComponent(&highlighter);
	cancelPendingUpdate();

	live = false;
}

bool CycleTour::compare(const Condition& cond, int value)
{
	switch(cond.oper)
	{
		case OperNull:			return true;
		case OperLessThan: 		return value < 	cond.value;
		case OperLessThanEQ: 	return value <= cond.value;
		case OperEquals: 		return value == cond.value;
		case OperMoreThan: 		return value > 	cond.value;
		case OperMoreThanEQ: 	return value >= cond.value;
		default:
			break;
	}

	return false;
}

bool CycleTour::conditionPassed(const Condition& c){
	jassert(c.oper != OperNull);
	jassert(c.type != NoCompare);

	switch(c.type) {
		case NoCompare:
			return true;

		case NumLines:{
			if(Interactor* itr = areaToInteractor(c.area)) {
				return compare(c, itr->getMesh()->getNumCubes());
			}

			break;
		}

		case NumPoints: {
			if(Interactor* itr = areaToInteractor(c.area)) {
				return compare(c, itr->getMesh()->getNumVerts());
			}

			break;
		}

		case CurrentLayer:{
			auto& lib = getObj(MeshLibrary);

			switch(c.area) {
				case AreaWshpEditor:
				case AreaWfrmWaveform3D:
					return compare(c, lib.getCurrentIndex(LayerGroups::GroupTime));
					break;
				case AreaSpectrogram:
				case AreaSpectrum:
					return compare(c, lib.getCurrentIndex(getSetting(MagnitudeDrawMode) ? LayerGroups::GroupSpect: LayerGroups::GroupPhase));
					break;
				case AreaScratch:
					return compare(c, lib.getCurrentIndex(LayerGroups::GroupScratch));
				default:
					return false;
			}
		}

		case NumLayers: {
			if(Interactor* itr = areaToInteractor(c.area))
				return compare(c, getObj(MeshLibrary).getGroup(itr->layerType).size());

			break;
		}

		case DfrmAssignments: {
			int num = 0;
			if(Interactor* itr = areaToInteractor(c.area)) {
				Mesh* mesh = itr->getMesh();

				for (auto* cube : mesh->getCubes()) {
					for(int j = 0; j <= Vertex::Curve; ++j)
						num += int(cube->deformerAt(j) >= 0);
				}

				return compare(c, num);
			}

			break;
		}
	}

	return true;
}

void CycleTour::performAction(Action& action) {
	Interactor* itr = areaToInteractor(action.area);

	switch(action.type) {
		case NullAction: break;

		case SetNoteOn: {
			if(! action.hasExecuted)
				getObj(AudioHub).getKeyboardState().noteOn(1, action.data1, action.value);

			action.hasExecuted = true;

			break;
		}

		case SetNoteOff: {
			getObj(AudioHub).getKeyboardState().noteOff(1, action.data1, 0.8f);

			break;
		}

		case StartPlayback: {
			getObj(GeneralControls).setPlayStarted();
			break;
		}

		case SwitchToEnv: {
			if(action.id != IdNull)
				getObj(EnvelopeInter2D).switchedEnvelope(action.id, true);

			break;
		}

		case SwitchToTool: {
			if(action.id != IdNull) {
				getSetting(Tool) = action.id;
				getObj(GeneralControls).updateHighlights();
			}

			break;
		}

		case ShowArea: {
			if(action.area == AreaModMatrix) {
				getObj(Dialogs).showModMatrix();
				startTimer(ToFrontId, 100);
			}

			break;
		}

		case HideArea: {
			if(action.area == AreaModMatrix) {
				getObj(ModMatrix).removeFromDesktop();
				getObj(ModMatrix).setVisible(false);
				startTimer(ToFrontId, 100);

				//Util::removeModalParent<DialogWindow>(&getObj(ModMatrix));
			}

			break;
		}

		case SetMorphRange: {
			getObj(MorphPanel).updateModPosition(action.id, action.value);

//			if(action.id != getSetting(CurrentMorphAxis))
//				doUpdate(ModUpdate);

			break;
		}

		case SetDeclick: {
			getDocSetting(Declick) = action.id == IdOn;
			break;
		}

		case SetPropsValue: {
			getObj(VertexPropertiesPanel).triggerSliderChange(action.id, action.value);
			break;
		}

		case SwitchMode: {
			switch(action.area) {
				case AreaSpectrum:
				case AreaSpectrogram:
					getObj(Spectrum3D).modeChanged(action.id == IdModeMagn, true);
					getObj(Spectrum3D).setIconHighlightImplicit();
					break;

				case AreaUnison:
					getObj(UnisonUI).triggerModeChanged(action.id == IdModeUniGroup);
					break;
				default:
					break;
			}
			break;
		}

		case SetPlaybackPos: {
			getObj(PlaybackPanel).setProgress(action.value, true);
			break;
		}

		case SetLayerMode: {
			jassert(action.area == AreaSpectrum || action.area == AreaSpectrogram);

			getObj(Spectrum3D).triggerButton(action.id);
			break;
		}

		case SetMatrixCell: {
			jassert(action.area == AreaModMatrix);

			int colourId = action.id == Vertex::Time ? ModMatrixPanel::YellowDim :
						   action.id == Vertex::Red ? ModMatrixPanel::RedDim : ModMatrixPanel::BlueDim;
			getObj(ModMatrixPanel).setMatrixCell(action.data1, action.data2, colourId);
			break;
		}

		case Zoom: {
			if(auto* panel = dynamic_cast<Panel*>(getTourGuide(action.area))) {
				panel->triggerZoom(action.id == IdZoomIn);
			}

			break;
		}

		case LinkRange: {
			if(action.id == IdYellow && ! getSetting(LinkYellow)) {
				getObj(MorphPanel).triggerClick(IdBttnLinkY);
			} else if(action.id == IdRed && ! getSetting(LinkRed)) {
				getObj(MorphPanel).triggerClick(IdBttnLinkR);
			} else if(action.id == IdBlue && ! getSetting(LinkBlue)) {
				getObj(MorphPanel).triggerClick(IdBttnLinkB);
			}

			break;
		}

		case UnlinkRange: {
			if(action.id == IdYellow && getSetting(LinkYellow)) {
				getObj(MorphPanel).triggerClick(IdBttnLinkY);
			} else if(action.id == IdRed && getSetting(LinkRed)) {
				getObj(MorphPanel).triggerClick(IdBttnLinkR);
			} else if(action.id == IdBlue && getSetting(LinkBlue)) {
				getObj(MorphPanel).triggerClick(IdBttnLinkB);
			}

			break;
		}

		case DeformLine: {
			if(action.hasExecuted) {
				return;
			}

			if(itr) {
				Mesh* mesh = itr->getMesh();

				if(isPositiveAndBelow(action.data1, mesh->getNumCubes()))
				{
					VertCube* cube = mesh->getCubes()[action.data1];

					cube->deformerAt(action.id) = action.data2;
					getObj(VertexPropertiesPanel).setSelectedAndCaller(itr);
					itr->triggerRefreshUpdate();
				}
			}

			action.hasExecuted = true;

			break;
		}

		case Enable:
		{
			switch(action.area) {
				case AreaWfrmWaveform3D:
				case AreaWshpEditor:
					if(! getObj(MeshLibrary).getCurrentProps(LayerGroups::GroupTime)->active)
						getObj(Waveform3D).triggerButton(IdBttnEnable);

					break;

				case AreaSpectrum:
				case AreaSpectrogram:
					if(! getObj(MeshLibrary).getCurrentProps(LayerGroups::GroupSpect)->active)
						getObj(Spectrum3D).triggerButton(IdBttnEnable);

					break;

				case AreaEnvelopes:
					if(! getObj(MeshLibrary).getCurrentEnvProps(getSetting(CurrentEnvGroup))->active)
						getObj(EnvelopeInter2D).triggerButton(IdBttnEnable);

					break;

				case AreaWaveshaper:	getObj(WaveshaperUI).setEffectEnabled(true); 			break;
				case AreaImpulse:		getObj(IrModellerUI).setEffectEnabled(true);			break;
				case AreaUnison:		getObj(UnisonUI).setEffectEnabled(true, true, true);	break;
				case AreaEQ:			getObj(EqualizerUI).setEffectEnabled(true, true, true);	break;
				case AreaReverb:		getObj(ReverbUI).setEffectEnabled(true, true, true);	break;
				case AreaDelay:			getObj(DelayUI).setEffectEnabled(true, true, true);		break;
				default:
					break;
			}
			break;
		}

		case Disable: {
			switch(action.area) {
				case AreaWfrmWaveform3D:
				case AreaWshpEditor:
					if(getObj(MeshLibrary).getCurrentProps(LayerGroups::GroupTime)->active)
						getObj(Waveform3D).triggerButton(IdBttnEnable);

					break;

				case AreaSpectrum:
				case AreaSpectrogram:
					if(getObj(MeshLibrary).getCurrentProps(LayerGroups::GroupSpect)->active)
						getObj(Spectrum3D).triggerButton(IdBttnEnable);
					break;

				case AreaEnvelopes:
					if(getObj(MeshLibrary).getCurrentProps(getSetting(CurrentEnvGroup))->active)
						getObj(EnvelopeInter2D).triggerButton(IdBttnEnable);
					break;

				case AreaWaveshaper:	getObj(WaveshaperUI).setEffectEnabled(false); 				break;
				case AreaImpulse:		getObj(IrModellerUI).setEffectEnabled(false);				break;
				case AreaUnison:		getObj(UnisonUI).setEffectEnabled(false, true, true);		break;
				case AreaEQ:			getObj(EqualizerUI).setEffectEnabled(false, true, true);	break;
				case AreaReverb:		getObj(ReverbUI).setEffectEnabled(false, true, true);		break;
				case AreaDelay:			getObj(DelayUI).setEffectEnabled(false, true, true);		break;
				default:
					break;
			}
			break;
		}

		case TriggerButton: {
			if(action.id < 0 || action.hasExecuted)
				return;

			switch(action.area) {
				case AreaSpectrum:
				case AreaSpectrogram:	getObj(Spectrum3D).triggerButton(action.id);		break;

				case AreaWshpEditor:
				case AreaWfrmWaveform3D:getObj(Waveform3D).triggerButton(action.id);		break;

				case AreaDeformers:		getObj(DeformerPanel).triggerButton(action.id);		break;
				case AreaScratch:		getObj(EnvelopeInter2D).triggerButton(action.id);	break;
				default:
					break;
			}

			action.hasExecuted = true;
			break;
		}

		case SetLayer: {
			switch(action.area) {
				case AreaWfrmWaveform3D:
				case AreaWshpEditor:
					getObj(Waveform3D).getPanelControls()->layerSelector->clickedOnRow(action.data1);
					break;

				case AreaSpectrogram:
				case AreaSpectrum:
					getObj(Spectrum3D).getPanelControls()->layerSelector->clickedOnRow(action.data1);
					break;

				case AreaDeformers:
					getObj(DeformerPanel).getPanelControls()->layerSelector->clickedOnRow(action.data1);
					break;

				case AreaScratch:
					getObj(EnvelopeInter2D).getScratchSelector()->clickedOnRow(action.data1);
					break;

				default:
					break;
			}
			break;
		}

		case OpenFactoryPreset: {
			if(! action.hasExecuted && action.str.isNotEmpty()) {
				getObj(FileManager).openFactoryPreset(action.str);
				action.hasExecuted = true;
			}

			break;
		}

		case AddPoint: {
			if(action.hasExecuted)
				return;

			if (itr) {
				ScopedBooleanSwitcher sbs(itr->suspendUndo);

				itr->addNewCube(0, action.point.x, action.point.y, action.value);

				if (action.triggersUpdate) {
					itr->postUpdateMessage();
					itr->doExtraMouseUp();
				}

				action.hasExecuted = true;
			}

			break;
		}

		case DeletePoint: {
			if(action.hasExecuted)
				return;

			if(itr) {
				Mesh* mesh = itr->getMesh();

				if(isPositiveAndBelow(action.data1, mesh->getNumVerts())) {
					vector<Vertex*>& selected = itr->getSelected();
					selected.clear();

					Vertex* vert = mesh->getVerts()[action.data1];

					selected.push_back(vert);
					itr->eraseSelected();

					if(action.triggersUpdate) {
						itr->postUpdateMessage();
						itr->doExtraMouseUp();
					}

					action.hasExecuted = true;
				}
			}

			break;
		}

		case MovePoint: {
			if(action.hasExecuted)
				return;

			if(itr) {
				Mesh* mesh = itr->getMesh();

				if(isPositiveAndBelow(action.data1, (int) mesh->getNumVerts())) {
					vector<Vertex*>& selected = itr->getSelected();
					selected.clear();

					Vertex* vert = mesh->getVerts()[action.data1];
					selected.push_back(vert);

					itr->updateSelectionFrames();
					itr->moveSelectedVerts(action.point);

					if(action.triggersUpdate) {
						getObj(VertexPropertiesPanel).setSelectedAndCaller(itr);
						itr->postUpdateMessage();
					}

					action.hasExecuted = true;
				}
			}

			break;
		}

		case SetVertexParam: {
			if (itr) {
				Mesh* mesh = itr->getMesh();

				if(isPositiveAndBelow(action.data1, mesh->getNumVerts()) &&
						NumberUtils::within<int>(action.id, IdParamTime, IdParamSharp)) {
					Vertex* vert = mesh->getVerts()[action.data1];
					Array<Vertex*> movingVerts = itr->getVerticesToMove(vert->owners.getFirst(), vert);

					for(auto movingVert : movingVerts) {
						movingVert->values[action.id] = action.value;
					}

					if(action.triggersUpdate) {
						getObj(VertexPropertiesPanel).setSelectedAndCaller(itr);
						itr->postUpdateMessage();
					}
				}
			}
			break;
		}

		case SelectPoint: {
			if(itr) {
				Mesh* mesh = itr->getMesh();

				if(isPositiveAndBelow(action.data1, (int) mesh->getNumVerts())) {
					vector<Vertex*>& selected = itr->getSelected();
					Vertex* vert = mesh->getVerts()[action.data1];

					bool contained = false;
					for(auto& i : selected) {
						if(i == vert) {
							contained = true;
							break;
						}
					}

					if(! contained)
						selected.push_back(vert);

					itr->setMovingVertsFromSelected();
					getObj(VertexPropertiesPanel).setSelectedAndCaller(itr);
					itr->resizeFinalBoxSelection(true);
					itr->update(UpdateType::Update);
				}
			}

			break;
		}

        case DeselectPoints: {
            if (itr) {
				vector<Vertex*>& selected = itr->getSelected();
				selected.clear();

				if(action.triggersUpdate) {
					itr->resizeFinalBoxSelection(true);
					itr->update(UpdateType::Update);
					getObj(VertexPropertiesPanel).setSelectedAndCaller(itr);
				}
			}

			break;
		}

		case SetKnobValue: {
			switch(action.area) {
				case AreaUnison: 		getObj(UnisonUI)	.getParamGroup().setKnobValue(action.data1, action.value, action.triggersUpdate, false); break;
				case AreaReverb:		getObj(ReverbUI)	.getParamGroup().setKnobValue(action.data1, action.value, action.triggersUpdate, false); break;
				case AreaEQ: 			getObj(EqualizerUI) .getParamGroup().setKnobValue(action.data1, action.value, action.triggersUpdate, false); break;
				case AreaDelay: 		getObj(DelayUI)	  	.getParamGroup().setKnobValue(action.data1, action.value, action.triggersUpdate, false); break;
				case AreaImpulse: 		getObj(IrModellerUI).getParamGroup().setKnobValue(action.data1, action.value, action.triggersUpdate, false); break;
				case AreaWaveshaper: 	getObj(WaveshaperUI).getParamGroup().setKnobValue(action.data1, action.value, action.triggersUpdate, false); break;
				case AreaMasterCtrls: 	getObj(OscControlPanel).getParamGroup().setKnobValue(action.data1, action.value, action.triggersUpdate, false); break;
				default: break;
			}

			break;
		}

		case LoadRefSample:
			getObj(FileManager).openWave(File(action.str), Dialogs::DialogSource);
			break;

		case UnloadSample:
			getObj(FileManager).unloadWav(true);
			break;

		case SetVertexSize: {
			getSetting(UseLargerPoints) = action.data1 > 1;

			if(action.triggersUpdate)
				getObj(Updater).update(UpdateSources::SourceAll, UpdateType::Repaint);

			break;
		}

		case SetUseRange:
			switch(action.id) {
				case IdYellow:	getSetting(UseYellowDepth) 	= action.data1 > 0; break;
				case IdRed:		getSetting(UseRedDepth) 	= action.data1 > 0; break;
				case IdBlue:	getSetting(UseBlueDepth) 	= action.data1 > 0; break;
				default: break;
			}
			break;

		case SetViewStage:
			getObj(SynthMenuBarModel).triggerClick(action.id);
			break;

        case ImpLoadWave: {
			Dialogs::WaveOpenData data(&getObj(Dialogs),
									   &getObj(SynthAudioSource).getIrModeller().getWrapper(),
									   DialogActions::LoadImpulse, Dialogs::LoadIRWave);

			data.audioFile 	= File(action.str);
			Dialogs::openWaveCallback(1, data);
			break;
		}

		case ImpModelWave: {
			Dialogs::WaveOpenData data(&getObj(Dialogs),
									   &getObj(SynthAudioSource).getIrModeller().getWrapper(),
									   DialogActions::LoadImpulse, Dialogs::ModelIRWave);

			data.audioFile = File(action.str);
			Dialogs::openWaveCallback(1, data);
			break;
		}

		case ImpUnloadWave:
			getObj(SynthAudioSource).getIrModeller().setPendingAction(IrModeller::unloadWav);
			break;

		case ChangeViewMode:
			getObj(SampleUtils).waveOverlayChanged(action.id == IdViewWave);
			break;

		case SetAxeSize:{
			if(action.area != AreaSpectrogram && action.area != AreaWfrmWaveform3D)
				return;

			if(itr)
				itr->setAxeSize(action.value);

			break;
		}

		case ChopLines: {
			if(itr && ! action.hasExecuted &&
					(action.area == AreaSpectrogram || action.area == AreaWfrmWaveform3D) &&
					action.id != IdNull) {
				if(auto* i3d = dynamic_cast<Interactor3D*>(itr)) {
					jassert(action.value > 0);

					/*
					i3d->sliceLines(action.point + Vertex2(0, action.value),
					                action.point - Vertex2(0, action.value), action.id);
					*/

					action.hasExecuted = true;
				}
			} else {
				jassertfalse;
			}
			break;
		}

        case SelectVertAtPos: {
            if (itr) {
                Vertex* vert = itr->findClosestVertex(action.point);
				vector<Vertex*>& selected = itr->getSelected();

				selected.clear();
				selected.push_back(vert);

				if(action.triggersUpdate) {
					itr->setMovingVertsFromSelected();
					itr->resizeFinalBoxSelection(true);
					itr->update(UpdateType::Update);

					getObj(VertexPropertiesPanel).setSelectedAndCaller(itr);
				}
			}
		}
	}
}

void CycleTour::Highlighter::paint(Graphics& g) {
	Rectangle<int> r = getLocalBounds();

    g.setColour(Colour::fromHSV(0.6f, 0.5f, 1.0f, 0.55f));
    g.drawRect(r.expanded(1, 1), 3);
}

CycleTour::Highlighter::Highlighter(CycleTour* tour) : tour(tour) {
    setInterceptsMouseClicks(false, false);
}

void CycleTour::Highlighter::update(const Rectangle<int>& r) {
	setBounds(r);
	repaint();
}

void CycleTour::ItemComponent::paint(Graphics& g) {
	g.setColour(Colour::greyLevel(0.75));
    g.setFont(FontOptions(18, Font::bold));

	Util::drawTextLine(g, item.title, 10, 15, Justification::left);

	Rectangle<int> textBounds = getLocalBounds().reduced(5, 5);
	textBounds.removeFromTop(25);

	layout.draw(g, textBounds.toFloat());
}

CycleTour::ItemWrapper::ItemWrapper(SingletonRepo* repo) :
		SingletonAccessor(repo, "ItemWrapper")
	,	borderSpace (20)
	,	arrowSize (16.0f) {
	addAndMakeVisible(content);
	setAlwaysOnTop (true);
	Component::addToDesktop (ComponentPeer::windowIsTemporary);
}

void CycleTour::ItemWrapper::setItem(const Item &item) {
    content.setItem(item);

    Rectangle<int> area = item.getArea(&getObj(CycleTour));

    Rectangle<int> desktopArea = Desktop::getInstance().getDisplays().getDisplayForPoint(area.getCentre())->userArea;
    updatePosition(area, desktopArea);
}

void CycleTour::ItemComponent::setItem(const Item &item) {
    this->item = item;

    int size = item.text.length();
    int width = 150;

    if (size > 0) {
        float root = sqrtf((float) size);
        width = jmax(320, int(13 * root * 1.8f + 0.5f));
    }

    AttributedString string;
    string.append(item.text, FontOptions(18));
    string.setColour(Colour::greyLevel(0.75f));
	layout.createLayout(string, width - 10);

	setSize(width, jmax(80, (int) layout.getHeight() + 40));
	repaint();
}

bool CycleTour::ItemWrapper::keyPressed(const KeyPress &press) {
    auto& tour = getObj(CycleTour);
	int code = press.getKeyCode();
	juce_wchar c = press.getTextCharacter();

    if (tour.isLive()) {
        if (code == KeyPress::escapeKey)
            tour.exit();
        else if (code == KeyPress::leftKey || code == KeyPress::rightKey) {
            code == KeyPress::leftKey ? tour.showPrevious() : tour.showNext();
        } else if (c == '[' || c == ']') {
            c == '[' ? tour.showPrevious() : tour.showNext();
        }

    } else {
        return false;
    }

	return true;
}

void CycleTour::ItemWrapper::updatePosition (
		const Rectangle<int>& newAreaToPointTo,
		const Rectangle<int>& newAreaToFitIn) {
    targetArea = newAreaToPointTo;
    availableArea = newAreaToFitIn;

    Rectangle newBounds (content.getWidth()  + borderSpace * 2,
                              content.getHeight() + borderSpace * 2);

    const int hw = newBounds.getWidth() / 2;
    const int hh = newBounds.getHeight() / 2;
    const float hwReduced = (float) (hw - borderSpace * 2);
    const float hhReduced = (float) (hh - borderSpace * 2);
    const float arrowIndent = borderSpace - arrowSize;

    Point targets[4] = { Point ((float) targetArea.getCentreX(), (float) targetArea.getBottom()),
                         Point ((float) targetArea.getRight(),   (float) targetArea.getCentreY()),
                         Point ((float) targetArea.getX(),       (float) targetArea.getCentreY()),
                         Point ((float) targetArea.getCentreX(), (float) targetArea.getY()) };

    Line lines[4] = { Line (targets[0].translated (-hwReduced, hh - arrowIndent),    targets[0].translated (hwReduced, hh - arrowIndent)),
                      Line (targets[1].translated (hw - arrowIndent, -hhReduced),    targets[1].translated (hw - arrowIndent, hhReduced)),
                      Line (targets[2].translated (-(hw - arrowIndent), -hhReduced), targets[2].translated (-(hw - arrowIndent), hhReduced)),
                      Line (targets[3].translated (-hwReduced, -(hh - arrowIndent)), targets[3].translated (hwReduced, -(hh - arrowIndent))) };

    const Rectangle centrePointArea (newAreaToFitIn.reduced (hw, hh).toFloat());
    const Point targetCentre (targetArea.getCentre().toFloat());

    float nearest = 1.0e9f;

    for (int i = 0; i < 4; ++i) {
        Line<float> constrainedLine(centrePointArea.getConstrainedPoint(lines[i].getStart()),
                                    centrePointArea.getConstrainedPoint(lines[i].getEnd()));

        const Point centre(constrainedLine.findNearestPointTo(targetCentre));
        float distanceFromCentre = centre.getDistanceFrom(targets[i]);

        if (!intersects(centrePointArea, lines[i])) {
	        distanceFromCentre += 1000.0f;
        }

        if (distanceFromCentre < nearest) {
            nearest = distanceFromCentre;

            targetPoint = targets[i];
            newBounds.setPosition ((int) (centre.getX() - hw),
                                   (int) (centre.getY() - hh));
        }
    }

    setBounds (newBounds);
}

bool CycleTour::ItemWrapper::intersects(const Rectangle<float> &r, const Line<float> &l) {
    Point<float> ignored;
	return r.contains (l.getStart()) || r.contains (l.getEnd())
			|| l.intersects (Line (r.getTopLeft(),     r.getTopRight()), 	ignored)
			|| l.intersects (Line (r.getTopRight(),    r.getBottomRight()), ignored)
			|| l.intersects (Line (r.getBottomRight(), r.getBottomLeft()), 	ignored)
			|| l.intersects (Line (r.getBottomLeft(),  r.getTopLeft()), 	ignored);
}

void CycleTour::ItemWrapper::refreshPath() {
    repaint();
    background = Image();
    outline.clear();

    const float gap 			= 4.5f;
    const float cornerSize 		= 9.0f;
    const float cornerSize2 	= 2.0f * cornerSize;
    const float arrowBaseWidth 	= arrowSize * 0.7f;
    const float left 			= content.getX() - gap, top = content.getY() - gap;
	const float right			= content.getRight() + gap, bottom = content.getBottom() + gap;
    const float targetX 		= targetPoint.getX() - getX(), targetY = targetPoint.getY() - getY();

    outline.startNewSubPath (left + cornerSize, top);

    if (targetY <= top) {
        outline.lineTo(targetX - arrowBaseWidth, top);
        outline.lineTo(targetX, targetY);
        outline.lineTo(targetX + arrowBaseWidth, top);
    }

    outline.lineTo(right - cornerSize, top);
    outline.addArc(right - cornerSize2, top, cornerSize2, cornerSize2, 0, MathConstants<float>::pi * 0.5f);

    if (targetX >= right) {
        outline.lineTo(right, targetY - arrowBaseWidth);
        outline.lineTo(targetX, targetY);
        outline.lineTo(right, targetY + arrowBaseWidth);
    }

    outline.lineTo(right, bottom - cornerSize);
    outline.addArc(right - cornerSize2, bottom - cornerSize2, cornerSize2, cornerSize2, MathConstants<float>::pi * 0.5f, MathConstants<float>::pi);

    if (targetY >= bottom) {
        outline.lineTo(targetX + arrowBaseWidth, bottom);
        outline.lineTo(targetX, targetY);
        outline.lineTo(targetX - arrowBaseWidth, bottom);
    }

    outline.lineTo(left + cornerSize, bottom);
    outline.addArc(left, bottom - cornerSize2, cornerSize2, cornerSize2, MathConstants<float>::pi, MathConstants<float>::pi * 1.5f);

    if (targetX <= left) {
        outline.lineTo(left, targetY + arrowBaseWidth);
        outline.lineTo(targetX, targetY);
        outline.lineTo(left, targetY - arrowBaseWidth);
    }

    outline.lineTo(left, top + cornerSize);
    outline.addArc(left, top, cornerSize2, cornerSize2, MathConstants<float>::pi * 1.5f, MathConstants<float>::pi * 2.0f - 0.05f);

    outline.closeSubPath();
}

void CycleTour::ItemWrapper::paint(Graphics& g) {
    if (background.isNull()) {
        background = Image(Image::ARGB, getWidth(), getHeight(), true);
        Graphics g2(background);
        getObj(SynthLookAndFeel).drawCallOutBoxBackground(g2, outline);
    }

    g.setColour(Colours::black);
    g.drawImageAt(background, 0, 0);
}

void CycleTour::ItemWrapper::resized() {
    content.setTopLeftPosition(borderSpace, borderSpace);
    refreshPath();
}

void CycleTour::ItemWrapper::moved() {
    refreshPath();
}

void CycleTour::ItemWrapper::childBoundsChanged(Component*) {
    updatePosition(targetArea, availableArea);
}

void CycleTour::timerCallback(int id) {
    if (id == ToFrontId) {
        wrapper.toFront(true);
        stopTimer(id);
        return;
    }

    vector <Item> &items = current.items;

    if (!isPositiveAndBelow(currentItem, (int) items.size())) {
	    return;
    }

    Item &item = current.items[currentItem];

    if (!live || item.actionIndex >= item.actions.size()) {
        stopTimer(item.area);
        return;
    }

    Action &action = item.actions[item.actionIndex];

    performAction(action);

    item.actionIndex++;

    stopTimer(item.area);

    if (item.actionIndex < item.actions.size()) {
	    startTimer(item.area, action.delayMillis);
    }
}


TourGuide* CycleTour::getTourGuide(Area area) {
	switch(area) {
		case AreaWfrmWaveform3D:	return &getObj(Waveform3D);
		case AreaMorphPanel:		return &getObj(MorphPanel);
		case AreaVertexProps:		return &getObj(VertexPropertiesPanel);
		case AreaSpectrogram:		return &getObj(Spectrum3D);
		case AreaEnvelopes:			return &getObj(Envelope2D);
		case AreaGenControls:		return &getObj(GeneralControls);
		case AreaImpulse:			return &getObj(IrModellerUI);
		case AreaWaveshaper:		return &getObj(WaveshaperUI);
		case AreaDeformers:			return &getObj(DeformerPanel);
		case AreaUnison:			return &getObj(UnisonUI);
		case AreaModMatrix:			return &getObj(ModMatrixPanel);
		case AreaMasterCtrls:		return &getObj(OscControlPanel);
		default: break;
	}

	jassertfalse;
	return nullptr;
}

Component* CycleTour::getComponent(int which) {
	Panel* panel = areaToPanel(which);

	if(panel != nullptr) {
		return panel->getComponent();
	}

	switch(which) {
		case AreaMain:			return &getObj(MainPanel);
		case AreaMorphPanel:	return &getObj(MorphPanel);
		case AreaModMatrix:		return &getObj(ModMatrixPanel);
		case AreaPlayback:		return &getObj(PlaybackPanel);
		case AreaVertexProps:	return &getObj(VertexPropertiesPanel);
		case AreaGenControls:	return &getObj(GeneralControls);
		case AreaMasterCtrls:	return &getObj(OscControlPanel);
		case AreaSharpBand: 	return &getObj(DerivativePanel);
		case AreaUnison:		return &getObj(UnisonUI);
		case AreaReverb:		return &getObj(ReverbUI);
		case AreaDelay:			return &getObj(DelayUI);
		case AreaEQ:			return &getObj(EqualizerUI);
		case AreaConsole:		return dynamic_cast<Console*>(&getObj(IConsole));
		default: break;
	}

	jassertfalse;
	return nullptr;
}

Panel* CycleTour::areaToPanel(int which) {
	switch(which) {
		case AreaWshpEditor: 	return &getObj(Waveform2D);
		case AreaWfrmWaveform3D:return &getObj(Waveform3D);
		case AreaSpectrum:		return &getObj(Spectrum2D);
		case AreaSpectrogram:	return &getObj(Spectrum3D);
		case AreaEnvelopes:		
		case AreaVolume:		
		case AreaPitch:			
		case AreaScratch:		return &getObj(Envelope2D);
		case AreaWaveshaper:	return &getObj(WaveshaperUI);
		case AreaImpulse:		return &getObj(IrModellerUI);
		case AreaDeformers:		return &getObj(DeformerPanel);
		default: break;
	}

	return nullptr;
}

Interactor* CycleTour::areaToInteractor(int which) {
	Panel* panel = areaToPanel(which);

	if(panel != nullptr) {
		return panel->getInteractor();
	}

	return nullptr;
}

bool CycleTour::passesRequirements(const String& ignore, const String& require) {
	if(ignore.isEmpty() && require.isEmpty()) {
		return true;
	}

	if(platformSplit( require.containsIgnoreCase("mac"), ignore.containsIgnoreCase("mac"))) {
		return false;
	}

	if(platformSplit( ignore.containsIgnoreCase("windows"), require.containsIgnoreCase("windows"))) {
		return false;
	}

	if(formatSplit( require.containsIgnoreCase("plugin"), ignore.containsIgnoreCase("plugin"))) {
		return false;
	}

	if(getSetting(FirstLaunch) ? ignore.containsIgnoreCase("first-launch") : require.containsIgnoreCase("first-launch")) {
		return false;
	}

	return true;
}

void CycleTour::readAction(Action& action, XmlElement* actionElem) {
	String actionStr = actionElem->getStringAttribute("type", "NullAction");
	jassert(actionStrings.contains(actionStr));

	action.type		 = actionStrings[actionStr];

	String idString  = actionElem->getStringAttribute("id", "IdNull");
	jassert(idStrings.contains(idString));

	String actionArea = actionElem->getStringAttribute("area", {});

	if(actionArea.isNotEmpty() && areaStrings.contains(actionArea))
		action.area = areaStrings[actionArea];

	action.id		= idStrings[idString];
	action.point.x	= actionElem->getDoubleAttribute("point-x", 	 0.5);
	action.point.y	= actionElem->getDoubleAttribute("point-y", 	 0.0);
	action.data1	= actionElem->getIntAttribute	("data1", 		 0);
	action.data2	= actionElem->getIntAttribute	("data2",		 0);
	action.data3	= actionElem->getIntAttribute	("data3", 		 0);
	action.value	= actionElem->getDoubleAttribute("value", 		 0);
	action.str		= actionElem->getStringAttribute("str", 		 {});
	action.delayMillis = actionElem->getIntAttribute("delay-millis", 100);
	action.triggersUpdate = actionElem->getBoolAttribute("updates",  true);

	if(action.str.contains("%TUT")) {
		String tutDir = getObj(Directories).getTutorialDir();
		action.str = action.str.replace("%TUT", tutDir);
	}
}

bool CycleTour::readXML(const XmlElement* element) {
	if(element == nullptr)
		return false;

	String cmdString(platformSplit("CTRL", "CMD"));

//	Tutorial tutorial;
	current.items.clear();
	current.name = element->getStringAttribute("title", "Untitled");

	forEachXmlChildElementWithTagName(*element, itemElem, "Item") {
		Item item;

		String areaStr = itemElem->getStringAttribute("area", 	 "AreaNull");
		String targStr = itemElem->getStringAttribute("subArea", "TargNull");

		jassert(areaStrings.contains(areaStr));
		jassert(subareaStrings.contains(targStr));

		item.area 			= areaStrings	[areaStr];
		item.subArea 		= subareaStrings[targStr];

		item.title			= itemElem->getStringAttribute("name", 		"Untitled");
		item.text			= itemElem->getStringAttribute("text", 		{});

		String ignoreStr 	= itemElem->getStringAttribute("ignore", 	{});
		String requireStr	= itemElem->getStringAttribute("require", 	{});

		if(! passesRequirements(ignoreStr, requireStr)) {
			continue;
		}

		if(item.text.contains("\\n")) {
			item.text = item.text.replace("\\n", "\n");
		}

		if(item.text.contains("%CMD")) {
			item.text = item.text.replace("%CMD", cmdString);
		}

		if(XmlElement* condElem = itemElem->getChildByName("Condition")) {
			Condition cond;
			cond.area 		= item.area;
			cond.oper 		= condStrings	[condElem->getStringAttribute("operation", 	"OperNull")];
			cond.type 		= compareStrings[condElem->getStringAttribute("comparable", "NoCompare")];
			cond.value 		= condElem->getIntAttribute("value", 0);
			cond.failMsg 	= condElem->getStringAttribute("failMsg", "Condition failed, please retry.");

			jassert(cond.oper != OperNull);
			jassert(cond.type != NoCompare);

			item.condition = cond;
		}

		forEachXmlChildElementWithTagName(*itemElem, actionElem, "Action") {
			Action animAction;
			animAction.area = item.area;
			readAction(animAction, actionElem);
			item.actions.push_back(animAction);
		}

		current.items.push_back(item);
	}

	enter();

	return true;
}

