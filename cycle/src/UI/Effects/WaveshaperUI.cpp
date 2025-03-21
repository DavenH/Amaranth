#include <UI/IConsole.h>
#include <App/EditWatcher.h>
#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Curve/IDeformer.h>
#include <UI/MiscGraphics.h>
#include <UI/Widgets/Knob.h>

#include "WaveshaperUI.h"
#include "../CycleDefs.h"
#include "../Dialogs/PresetPage.h"
#include "../Widgets/Controls/MeshSelector.h"
#include "../Widgets/Controls/Spacers.h"
#include "../../App/CycleTour.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../UI/VertexPanels/DeformerPanel.h"

const int WaveshaperUI::oversampFactors[5] = { 1, 1, 2, 4, 8 };

WaveshaperUI::WaveshaperUI(SingletonRepo* repo) :
        EffectPanel		(repo, "WaveshaperUI", true)
    ,	SingletonAccessor(repo, "WaveshaperUI")
    ,	Worker			(repo, "WaveshaperUI")
    ,	title			(repo, "WAVESHAPER")
    ,	enabledButton	(5, 5, this, repo, "Enable effect")
    ,	controls		(this, repo, true)
    ,	isEnabled		(false)
{
}

void WaveshaperUI::init() {
    updateSource			= UpdateSources::SourceWaveshaper;
    layerType 				= LayerGroups::GroupWaveshaper;
    curveIsBipolar 			= false;
    vertsAreWaveApplicable 	= true;
    nameCornerPos			= juce::Point<float>(-25, 25);

    float pad = getRealConstant(WaveshaperPadding);

    bgPaddingRight 	= pad;
    bgPaddingLeft 	= pad;
    bgPaddingTop 	= pad;
    bgPaddingBttm 	= pad;

    zoomPanel->rect.x = 0.5f * pad;
    zoomPanel->rect.w = 1.0f - pad;
    zoomPanel->rect.y = 0.5f * pad;
    zoomPanel->rect.h = 1.0f - pad;

    oversampLabel.setText		("AA factor", dontSendNotification);
    oversampLabel.setEditable	(false, false);
    oversampLabel.setColour		(Label::textColourId, Colour::greyLevel(0.65f));
    oversampLabel.setFont		(*getObj(MiscGraphics).getSilkscreen());
    oversampLabel.setBorderSize(BorderSize<int>());

    oversampleBox.addItem("1", 1);
    oversampleBox.addItem("2", 2);
    oversampleBox.addItem("4", 3);
    oversampleBox.addItem("8", 4);
    oversampleBox.addListener(this);
    oversampleBox.setSelectedId(1, dontSendNotification);

    String names[] = { "Pre", "Post" };
    Knob* preampKnob, *postampKnob;

    paramGroup->addSlider(preampKnob = new Knob(repo, Waveshaper::Preamp, "Preamp", 0.5f));
    paramGroup->addSlider(postampKnob = new Knob(repo, Waveshaper::Postamp, "Postamp", 0.5f));

    using namespace Ops;
    StringFunction decibel30 = StringFunction(0).mul(2.f).sub(1.f).mul(30.f);
    StringFunction decibel45 = StringFunction(0).mul(2.f).sub(1.f).mul(45.f);

    preampKnob->setStringFunctions(decibel45,  decibel45.withPostString(" dB"));
    postampKnob->setStringFunctions(decibel30, decibel30.withPostString(" dB"));

    int i = 0;
    for(auto& label : labels) {
        label.setText		(names[i], dontSendNotification);
        label.setEditable	(false, false);
        label.setFont		(*getObj(MiscGraphics).getSilkscreen());
        label.setColour		(Label::textColourId, Colour::greyLevel(0.55f));
        label.setMinimumHorizontalScale(1.f);
        label.setBorderSize(BorderSize<int>());
        ++i;
    }

    paramGroup->listenToKnobs();
    createNameImage("Waveshaper", false, true);

    waveshaper = &getObj(SynthAudioSource).getWaveshaper();
    waveshaper->setRasterizer(rasterizer);

    rasterizer->setDeformer(&getObj(DeformerPanel));
    rasterizer->setMesh(getMesh());
    rasterizer->performUpdate(Update);

    selector = std::make_unique<MeshSelector<Mesh>>(repo, this, "ws", false, true, true);

    controls.addDynamicSizeComponent(new ClearSpacer(8, false));
    controls.addButton(&enabledButton);
    controls.addDynamicSizeComponent(new ClearSpacer(10));
    controls.addDynamicSizeComponent((paramGroup->getKnob<Knob>(Waveshaper::Preamp)), false);
    controls.addDynamicSizeComponent(new ClearSpacer(4));
    controls.addDynamicSizeComponent((paramGroup->getKnob<Knob>(Waveshaper::Postamp)), false);
    controls.addDynamicSizeComponent(new ClearSpacer(80, false));
    controls.addDynamicSizeComponent(selector.get(), false);

    for (int i = 0; i < (int) paramGroup->getNumParams(); ++i) {
        controls.addAndMakeVisible(paramGroup->getKnob<juce::Component>(i));
        controls.addAndMakeVisible(&labels[i]);
    }

    controls.addAndMakeVisible(selector.get());
    controls.addAndMakeVisible(&oversampLabel);
    controls.addAndMakeVisible(&oversampleBox);
}

void WaveshaperUI::preDraw() {
}

void WaveshaperUI::postCurveDraw() {
    int left = 0;
    int right = getWidth();
    int innerRight = sx(1 - getRealConstant(WaveshaperPadding));
    int innerLeft = sx(getRealConstant(WaveshaperPadding));

    int bottom = 0;
    int low = sy(1 - getRealConstant(WaveshaperPadding));
    int high = sy(getRealConstant(WaveshaperPadding));
    int top = getHeight();

    gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);

    gfx->fillRect(innerLeft, top, innerRight, high, false);
    gfx->fillRect(innerLeft, bottom, innerRight, low, false);
    gfx->fillRect(left, top, innerLeft, bottom, false);
    gfx->fillRect(right, top, innerRight, bottom, false);

    gfx->setCurrentColour(0.3f, 0.3f, 0.3f, 0.5f);
    gfx->setCurrentLineWidth(1.f);
    gfx->disableSmoothing();

    gfx->drawRect(innerLeft, high, innerRight, low, false);
    gfx->enableSmoothing();
}

void WaveshaperUI::setMeshAndUpdate(Mesh* mesh) {
    mesh->updateToVersion(ProjectInfo::versionNumber);

    rasterizer->cleanUp();
    rasterizer->setMesh(mesh);
    rasterizer->performUpdate(Update);
    waveshaper->rasterizeTable();

    repaint();
}

void WaveshaperUI::setCurrentMesh(Mesh* mesh) {
    setMeshAndUpdate(mesh);

    getObj(EditWatcher).setHaveEditedWithoutUndo(true);
    getObj(MeshLibrary).setCurrentMesh(layerType, mesh);

    clearSelectedAndCurrent();
    postUpdateMessage();
}

void WaveshaperUI::previewMesh(Mesh* mesh) {
    ScopedLock sl(panel->getRenderLock());

    setMeshAndUpdate(mesh);
}

void WaveshaperUI::previewMeshEnded(Mesh* oldMesh) {
    ScopedLock sl(panel->getRenderLock());

    setMeshAndUpdate(oldMesh);
}

void WaveshaperUI::enterClientLock() {
    getObj(SynthAudioSource).getLock().enter();
    renderLock.enter();
}

void WaveshaperUI::exitClientLock() {
    renderLock.exit();
    getObj(SynthAudioSource).getLock().exit();
}

String WaveshaperUI::getKnobName(int index) const {
    switch (index) {
        case Waveshaper::Postamp: return "Post";
        case Waveshaper::Preamp: return "Pre";
        default: break;
    }

    return String();
}

void WaveshaperUI::doGlobalUIUpdate(bool force) {
    Interactor::doGlobalUIUpdate(force);
}

void WaveshaperUI::reduceDetail() {
    Interactor::reduceDetail();
}

void WaveshaperUI::restoreDetail() {
    Interactor::restoreDetail();
}

bool WaveshaperUI::updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) {
    return waveshaper->doParamChange(knobIndex, knobValue, doFurtherUpdate);
}

void WaveshaperUI::updateDspSync() {
    rasterizer->performUpdate(Update);

    if (isEffectEnabled())
        waveshaper->rasterizeTable();
}

Mesh* WaveshaperUI::getCurrentMesh() {
    return Interactor::getMesh();
}

void WaveshaperUI::writeXML(XmlElement* registryElem) const {
    auto* waveshaperElem = new XmlElement(panelName);

    paramGroup->writeKnobXML(waveshaperElem);

    waveshaperElem->setAttribute("enabled", isEffectEnabled());
    waveshaperElem->setAttribute("oversampleFactor", waveshaper->getOversampleFactor());

    registryElem->addChildElement(waveshaperElem);
}

bool WaveshaperUI::readXML(const XmlElement* registryElem) {
    ScopedLock sl(renderLock);

    XmlElement* waveshaperElem = registryElem->getChildByName(panelName);

    if (waveshaperElem == nullptr) {
        return false;
    }

    isEnabled = waveshaperElem->getBoolAttribute("enabled", false);
    int factor = waveshaperElem->getIntAttribute("oversampleFactor", 1);

    enabledButton.setHighlit(isEnabled);
    waveshaper->setPendingOversampleFactor(factor);

    for (int i = 1; i < 5; ++i) {
        if (oversampFactors[i] == factor) {
            oversampleBox.setSelectedId(i, dontSendNotification);
        }
    }

    paramGroup->readKnobXML(waveshaperElem);

    return true;
}

void WaveshaperUI::panelResized() {
    Panel::panelResized();
    controls.resized();

    Rectangle<int> bounds = controls.getLocalBounds().reduced(0, 3);
    Rectangle<int> compBounds = comp->getLocalBounds();

    for (int i = 0; i < numElementsInArray(labels); ++i) {
        Knob* knob = paramGroup->getKnob<Knob>(i);
        knob->setTopLeftPosition(knob->getX(), 1);

        const Rectangle<int>& b = knob->getBounds();
        int width = roundToInt(Util::getStringWidth(labels[i].getFont(), labels[i].getText(false)));

        labels[i].setBounds(b.getX() + (b.getWidth() - width) / 2, b.getBottom(), width, 6);
    }

    int numParams = paramGroup->getNumParams();
    bounds.removeFromLeft(paramGroup->getKnob<Knob>(numParams - 1)->getRight());
    bounds.removeFromLeft(15);

    Rectangle<int> oversampBounds = bounds.removeFromLeft(70);
    oversampLabel.setBounds(oversampBounds.removeFromBottom(8));
    oversampBounds.removeFromBottom(1);
    oversampleBox.setBounds(oversampBounds.removeFromLeft(45));
    oversampleBox.setWantsKeyboardFocus(false);

    bounds.removeFromRight(6);
}

void WaveshaperUI::showCoordinates() {
    float invSize = 1.f / float(1.f - 2.f * getRealConstant(WaveshaperPadding));
    float xformX = (state.currentMouse.x - getRealConstant(WaveshaperPadding)) * invSize;
    float xformY = (state.currentMouse.y - getRealConstant(WaveshaperPadding)) * invSize;

    String message = String(xformX, 2) + ", " + String(xformY, 2);
    getObj(IConsole).write(message, IConsole::DefaultPriority);
}

void WaveshaperUI::buttonClicked(Button* button) {
    progressMark

    if (button == &enabledButton) {
        isEnabled ^= true;
        enabledButton.setHighlit(isEnabled);

        if (isEnabled)
            waveshaper->rasterizeTable();

        forceNextUIUpdate = true;
        triggerRefreshUpdate();
    }
}

bool WaveshaperUI::shouldTriggerGlobalUpdate(Slider* slider) {
    return isEffectEnabled();
}

void WaveshaperUI::doLocalUIUpdate() {
    repaint();
}

void WaveshaperUI::setEffectEnabled(bool enabled) {
    if (Util::assignAndWereDifferent(isEnabled, enabled)) {
        isEnabled = enabled;
        enabledButton.setHighlit(isEnabled);

        triggerRefreshUpdate();
    }
}

CriticalSection& WaveshaperUI::getClientLock() {
    return getObj(SynthAudioSource).getLock();
}

void WaveshaperUI::comboBoxChanged(ComboBox* box) {
    if (box == &oversampleBox) {
        int id = box->getSelectedId();
        waveshaper->setPendingOversampleFactor(oversampFactors[id]);
        getObj(EditWatcher).setHaveEditedWithoutUndo(true);

      #if PLUGIN_MODE
        getObj(PluginProcessor).updateLatency();
      #endif

        triggerRefreshUpdate();
    }
}

void WaveshaperUI::doubleMesh() {
    float padding = getRealConstant(WaveshaperPadding);

    getCurrentMesh()->twin(padding, padding);
    postUpdateMessage();
}

juce::Component* WaveshaperUI::getComponent(int which) {
    switch (which) {
        case CycleTour::TargWaveshaperOvsp: return &oversampleBox;
        case CycleTour::TargWaveshaperPre: return paramGroup->getKnob<Knob>(Waveshaper::Preamp);
        case CycleTour::TargWaveshaperPost: return paramGroup->getKnob<Knob>(Waveshaper::Postamp);
        case CycleTour::TargWaveshaperSlct: return selector.get();
        default:
            break;
    }

    return nullptr;
}
