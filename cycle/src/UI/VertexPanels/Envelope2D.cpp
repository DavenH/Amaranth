#include <Algo/AutoModeller.h>
#include <App/EditWatcher.h>
#include <App/Doc/PresetJson.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Curve/EnvRasterizer.h>
#include <Curve/EnvelopeMesh.h>
#include <Obj/ColorPos.h>
#include <UI/MiscGraphics.h>
#include <UI/Panels/CommonGfx.h>
#include <UI/Panels/Texture.h>
#include <UI/Panels/ZoomPanel.h>
#include <UI/Widgets/CalloutUtils.h>

#include "Envelope2D.h"
#include "Spectrum3D.h"
#include "Algo/Resampling.h"

#include "../CycleGraphicsUtils.h"
#include "../Panels/OscControlPanel.h"
#include "../Widgets/Controls/MeshSelector.h"

#include "../../Audio/SynthAudioSource.h"
#include "../../App/CycleTour.h"
#include "../../Curve/E3Rasterizer.h"
#include "../../Inter/EnvelopeInter2D.h"
#include "../../Inter/EnvelopeInter3D.h"
#include "../../UI/Panels/PlaybackPanel.h"

namespace {
    struct EnvelopeGroupMapping {
        int groupId;
        const char* xmlTag;
        const char* jsonKey;
        const char* currentIndexAttr;
    };

    const EnvelopeGroupMapping envelopeMappings[] = {
        { LayerGroups::GroupVolume, "VolumeProps", "volume", "volumeCurrentIndex" },
        { LayerGroups::GroupPitch, "PitchProps", "pitch", "pitchCurrentIndex" },
        { LayerGroups::GroupScratch, "ScratchProps", "scratch", "scratchCurrentIndex" },
        { LayerGroups::GroupWavePitch, "WavePitchProps", "wavePitch", "wavePitchCurrentIndex" }
    };

    var writeEnvelopeGroupJson(MeshLibrary& meshLibrary, const EnvelopeGroupMapping& mapping) {
        auto json = PresetJson::object();
        Array<var> layers;
        auto& group = meshLibrary.getLayerGroup(mapping.groupId);

        json->setProperty("currentLayer", group.current);

        for (const auto& layer : group.layers) {
            auto layerJson = PresetJson::object();

            layerJson->setProperty("mesh", layer.mesh->writeJSON());

            if (layer.props != nullptr) {
                layerJson->setProperty("properties", layer.props->writeJSON());
            }

            layers.add(PresetJson::toVar(layerJson));
        }

        json->setProperty("layers", var(layers));
        return PresetJson::toVar(json);
    }
}

Envelope2D::Envelope2D(SingletonRepo* repo) :
        SingletonAccessor(repo, "Envelope2D")
    ,	Panel2D		(repo, "Envelope2D", true, true)
    ,	controls	(repo, this)
    ,	envSelectCO	(nullptr)
    ,	envSelectPO	(nullptr)
    ,	loopCO		(nullptr)
    ,	loopPO		(nullptr)
    ,	scrollListener(this) {
}

Envelope2D::~Envelope2D() {
    controls.destroy();

    meshSelector = nullptr;
}

void Envelope2D::init() {
    Panel2D::init();

    vertPadding 		= 2;
    doesDrawMouseHint 	= true;
    curveIsBipolar 		= false;
    speedApplicable		= false;
    guideCurveApplicable	= false;
    e2Interactor		= &getObj(EnvelopeInter2D);
    setInteractor(e2Interactor);

    zoomPanel->rect.w = 1.;
    zoomPanel->tendZoomToRight = false;
    zoomPanel->rect.xMaximum = MathConstants<float>::sqrt2;

    colorA = Color(0.5f, 0.71f, 1.0f);
    colorB = Color(0.5f, 0.71f, 1.0f);

    createNameImage("Envelopes");

    meshSelector = std::make_unique<MeshSelector<EnvelopeMesh>>(repo, e2Interactor, "envelope", "env", true, false, false, false);

    EnvelopeInter2D* e2 = e2Interactor;

    juce::Component* tempEnv[] 	= { &e2->volumeIcon, &e2->scratchIcon, &e2->pitchIcon, 	&e2->wavePitchIcon  };
    juce::Component* tempLoop[] 	= { &e2->loopIcon, 	 &e2->sustainIcon };

    int envSize = numElementsInArray(tempEnv);

    CalloutUtils::addRetractableCallout(envSelectCO, envSelectPO, repo, 2, 4, tempEnv, envSize, &controls, false);
    CalloutUtils::addRetractableCallout(loopCO, loopPO, repo, 4, 3, tempLoop, numElementsInArray(tempLoop), &controls, false);

    controls.addAndMakeVisible(&e2->configIcon);
    controls.init();

    envSelectCO->addMouseListener(&scrollListener, true);
    setSelectorVisibility(false, false);
}

void Envelope2D::ScrollListener::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    float yInc = wheel.deltaY;

    if (panel->envSelectCO->isParentOf(e.eventComponent)) {
        int env = panel->getSetting(CurrentEnvGroup);

        int meshes[] = {
            LayerGroups::GroupVolume, LayerGroups::GroupScratch,
            LayerGroups::GroupPitch, LayerGroups::GroupWavePitch
        };

        if (yInc > 0) {
            for (int i = 1; i < numElementsInArray(meshes); ++i) {
                if (env == meshes[i]) {
                    panel->e2Interactor->switchedEnvelope(meshes[i - 1], true);
                    break;
                }
            }
        } else if (yInc < 0) {
            for (int i = 0; i < numElementsInArray(meshes) - 1; ++i) {
                if (env == meshes[i]) {
                    if (meshes[i + 1] == LayerGroups::GroupWavePitch && !panel->e2Interactor->wavePitchIcon.
                        isApplicable())
                        break;

                    panel->e2Interactor->switchedEnvelope(meshes[i + 1], true);
                    break;
                }
            }
        }
    }
}

void Envelope2D::drawBackground(bool fill) {
    Panel::drawBackground();

    float one = sx(1) - 1;
    float end = sxnz(1);

    if (end > one) {
        float top = sy(0);
        float bot = sy(1);

        gfx->setCurrentColour(0.17f, 0.1f, 0.14f, 0.3f);
        gfx->fillRect(one, top, end, bot, false);
    }
}

void Envelope2D::updateBackground(bool verticalOnly) {
    int currentEnv = getSetting(CurrentEnvGroup);
    MeshLibrary::EnvProps* props = getObj(MeshLibrary).getCurrentEnvProps(currentEnv);

    if (props == nullptr) {
        return;
    }

    if (props->logarithmic) {
        Panel::updateBackground(true);

        horzMinorLines.resize(16);
        float value = 1.f;

        for (int i = 0; i < 16; ++i) {
            float level = (value *= 0.5);
            horzMinorLines[i] = Arithmetic::logMapping(30, level);
        }
    } else {
        Panel::updateBackground(false);
    }

    repaint();
}

void Envelope2D::drawRatioLines() {
}

void Envelope2D::panelResized() {
    Panel::panelResized();
}

void Envelope2D::dimensionsChanged() {
}

void Envelope2D::drawCurvesAndSurfaces() {
    EnvRasterizer* rast = e2Interactor->getEnvRasterizer();

    if (rast == nullptr) {
        Panel2D::drawCurvesAndSurfaces();
        return;
    }

    RasterizerData& data = rast->getRastData();
    ScopedLock dataLock(data.lock);

    bool reduceAlpha = ! isMeshEnabled();

    const Color color1(0.5f, 0.71f, 1.0f, 	reduceAlpha ? 0.45f : 0.75f);
    const Color color2(0.6f, 0.45f, 0.25f, 	reduceAlpha ? 0.45f : colorB.alpha());
    const Color color3(0.5f, 0.2f, 0.25f, 	reduceAlpha ? 0.45f : colorB.alpha());

    Buffer<float> waveX, waveY, a;
    bool samplesAreBipolar = false;
    bool isLast 		   = false;
    float stopPosition 	   = 1;

    {
        ScopedLock sl(data.lock);

        const vector<Intercept>& icpts  = data.intercepts;
        waveX = data.waveX;
        waveY = data.waveY;

        float backEx = icpts.empty() ? 1.f : icpts.back().x;
        stopPosition = sx(backEx);

        if(waveX.empty() || icpts.empty())
            return;

        int istart = jmax(0, data.zeroIndex - 4);
        int size = waveX.size() - istart;

        prepareBuffers(size, size);

        xy.copyFrom(waveX.offset(istart), waveY.offset(istart));

        a = cBuffer.withSize(size);
    }

    VecOps::mul(xy.y, 0.25f, a);
    a.abs().subCRev(0.5f).mul(color1.alpha());

    applyScale(xy);

    Color topColors[] = { color1, color2, color3 };
    Color sectionClr = color1;
    ColorPos curr;

    vector<ColorPos> positions;
    positions.reserve(xy.size() + 10);

    float baseY 	= sy(curveIsBipolar ? 0.5f : 0.f);
    float baseAlpha	= color1.alpha() * 0.2f;

    float loopStart, sustain;
    getLoopPoints(loopStart, sustain);

    // fixes transition if there's no loop
    if(loopStart < 0) {
        topColors[1] = color1;
    }

    float changePoints[] = { sx(loopStart), sx(sustain) };

    int size = xy.size();
    int i = 0;
    while(i < size - 1 && xy.x[i + 1] < 0) {
        ++i;
    }

    int cp = 0;
    while(xy.x[i] > changePoints[cp] && cp < numElementsInArray(changePoints)) {
        ++cp;
    }

    sectionClr = topColors[cp];

    curr.update(xy.x[i] - 5, xy.y[i], sectionClr.withAlpha(a.front()));
    positions.push_back(curr);

    gfx->setCurrentLineWidth(interactor->mouseFlag(WithinReshapeThresh) ? 2.f : 1.f);

    for (; i < size; ++i) {
        isLast = i == size - 1;
        samplesAreBipolar = curveIsBipolar && !isLast && (xy.y[i] - baseY) * (xy.y[i + 1] - baseY) < 0;

        curr.update(xy[i], sectionClr.withAlpha(a[i]));
        positions.push_back(curr);

        for (int j = 0; !isLast && j < numElementsInArray(changePoints); ++j) {
            float val = changePoints[j];

            if (NumberUtils::within(val, xy.x[i], xy.x[i + 1])) {
                Color& top1 	= topColors[j];
                Color& top2 	= topColors[j + 1];

                float lerpY 	= Resampling::lerp(xy[i], xy[i + 1], val);
                float righter 	= val + 0.0001f;

                if (j < 1) {
                    curr.update(val, lerpY, top1.withAlpha(a[i]));
                    positions.push_back(curr);
                }

                curr.update(righter, lerpY, top2.withAlpha(a[i]));
                positions.push_back(curr);

                sectionClr = top2;
            }
        }

        if (!isLast && NumberUtils::within(stopPosition, xy.x[i], xy.x[i + 1])) {
            float lerpY = Resampling::lerp(xy[i], xy[i + 1], stopPosition);

            curr.update(stopPosition, lerpY, sectionClr.withAlpha(a[i]));
            positions.push_back(curr);

            break;
        }

        //zero wave2Ding
        if (samplesAreBipolar) {
            float m 	= (xy.y[i + 1] - xy.y[i]) / (xy.x[i + 1] - xy.x[i]);
            float icptX = (m * xy.x[i] - xy.y[i] + baseY) / m;

            curr.update(icptX, baseY, sectionClr.withAlpha(a[i]));
            positions.push_back(curr);
        }
    }

    if(! positions.empty()) {
        gfx->fillAndOutlineColoured(positions, baseY, baseAlpha, true, true);
    }

    gfx->setCurrentLineWidth(1.f);
}

void Envelope2D::componentChanged() {
    Panel::componentChanged();
}

void Envelope2D::drawMouseHint() {
    drawVerticalLine();
}

void Envelope2D::getLoopPoints(float& loopStart, float& sustain) {
    if (EnvRasterizer* rast = e2Interactor->getEnvRasterizer()) {
        int loopIdx, sustIdx;
        rast->getIndices(loopIdx, sustIdx);

        RasterizerData& data = rast->getRastData();
        ScopedLock dataLock(data.lock);

        const vector<Intercept>& icpts = data.intercepts;

        loopStart = -1;
        sustain = -1;

        if(isPositiveAndBelow(loopIdx, (int) icpts.size())) {
            loopStart = icpts[loopIdx].x;
        }

        if(isPositiveAndBelow(sustIdx, (int) icpts.size())) {
            sustain = icpts[sustIdx].x;
        }
    }
}

void Envelope2D::zoomAndRepaint() {
    contractToRange(true);
    zoomPanel->panelZoomChanged(false);
    repaint();
}

void Envelope2D::setSelectorVisibility(bool isVisible, bool doRepaint) {
    controls.setSelectorVisibility(isVisible, doRepaint);
}


Envelope2D::Controls::Controls(SingletonRepo* repo, Envelope2D* panel) :
        SingletonAccessor(repo, "Envelope2DControls")
    ,	panel				(panel)
    ,	showLayerSelector	(false)
{
}

void Envelope2D::Controls::paintOverChildren(Graphics& g) {
    Image pullout = getObj(MiscGraphics).getIcon(7, 6);
    Component& icon = getObj(EnvelopeInter2D).configIcon;

    g.drawImageAt(pullout, icon.getX(), icon.getY());
}

void Envelope2D::Controls::setSelectorVisibility(bool isVisible, bool doRepaint) {
    showLayerSelector = isVisible;
    EnvelopeInter2D* e2 = panel->e2Interactor;

    if (isVisible) {
        addAndMakeVisible(&e2->addRemover);
        addAndMakeVisible(&e2->layerSelector);
    } else {
        removeChildComponent(&e2->addRemover);
        removeChildComponent(&e2->layerSelector);
    }

    resized();

    if (doRepaint) {
        repaint();
    }
}

void Envelope2D::Controls::destroy() {
    removeChildComponent(&panel->e2Interactor->addRemover);
    removeChildComponent(&panel->e2Interactor->layerSelector);
    removeChildComponent(panel->meshSelector.get());
}

void Envelope2D::Controls::init() {
    EnvelopeInter2D* e2 = panel->e2Interactor;

    addAndMakeVisible(panel->meshSelector.get());
    addAndMakeVisible(e2->getEnableButton());
}

void Envelope2D::Controls::paint(Graphics& g)
{
    getObj(CycleGraphicsUtils).fillBlackground(this, g);

    EnvelopeInter2D* e2 = panel->e2Interactor;
    Array<Rectangle<int> > rects;

    rects.add(panel->envSelectCO->getBoundsInParentDelegate().translated(0, -1));
    rects.add(panel->loopCO->getBoundsInParentDelegate().translated(0, -1));
    rects.add(e2->getEnableButton()->getBoundsInParent().expanded(0, 1));

    if(showLayerSelector) {
        rects.add(e2->addRemover.getBoundsInParentDelegate());
        rects.add(e2->layerSelector.getBoundsInParentDelegate());
    }

    for (auto rect: rects) {
        Path strokePath;
        strokePath.addRoundedRectangle(rect.getX(), rect.getY() + 1, rect.getWidth() + 1,
                                       rect.getHeight() - 2, 2.f);
        strokePath.applyTransform(AffineTransform::translation(- 0.5f, - 0.5f));

        g.setColour(Colours::black);
        g.strokePath(strokePath, PathStrokeType(1.f));
    }
}

void Envelope2D::Controls::resized()
{
    EnvelopeInter2D* e2 		= panel->e2Interactor;
    Button* b 					= e2->getEnableButton();

    Rectangle<int> bounds 		= getLocalBounds().reduced(4, 4);
    Rectangle<int> layerBounds 	= bounds;
    Rectangle<int> leftSide 	= layerBounds.removeFromLeft(24);
    Rectangle<int> rightSide 	= layerBounds.removeFromRight(24);

    leftSide.removeFromTop(1);
    b->setBounds(leftSide.removeFromTop(25));
    leftSide.removeFromTop(8);
    rightSide.removeFromTop(1);

    panel->envSelectCO->setBounds(leftSide.removeFromTop(panel->envSelectCO->getExpandedSize()));

    panel->loopCO->setBounds(rightSide.removeFromTop(panel->loopCO->getExpandedSize()));

    if (showLayerSelector) {
        rightSide.removeFromTop(8);
        e2->addRemover.setBounds(rightSide.removeFromTop(e2->addRemover.getExpandedSize()));
        rightSide.removeFromTop(4);

        e2->layerSelector.setBounds(rightSide.removeFromTop(e2->layerSelector.getExpandedSize()));
    }

    int leftHeight 	= layerBounds.getHeight() - leftSide.getHeight();
    int rightHeight = layerBounds.getHeight() - rightSide.getHeight();

    bounds.removeFromTop(jmax(leftHeight, rightHeight));
    if(bounds.getHeight() > 40) {
        bounds.removeFromTop(8);
    }

    panel->meshSelector->setBounds(bounds.removeFromLeft(24).removeFromTop(24));
    e2->configIcon.setBounds(bounds.removeFromRight(24).removeFromTop(24));
}


void Envelope2D::createScales()
{
    info("creating scales for " << panelName << "\n");

    int currentGroup = getSetting(CurrentEnvGroup);
    MeshLibrary::EnvProps* props = getObj(MeshLibrary).getCurrentEnvProps(currentGroup);

    if(props == nullptr) {
        return;
    }

    vector<Rectangle<float> > newScales;

    int fontScale 	 = getSetting(PointSizeScale);
    auto& mg         = getObj(MiscGraphics);
    Font& font 		 = *mg.getAppropriateFont(fontScale);
    float lengthSecs = getObj(OscControlPanel).getLengthInSeconds();
    float tempoScale = getObj(SynthAudioSource).getTempoScale();
    int position 	 = 0;
    int beats 		 = 4;
    float alpha 	 = fontScale == ScaleSizes::ScaleSmall ? 0.45f : 0.78f;

  #if PLUGIN_MODE
    AudioPlayHead::CurrentPositionInfo info = getObj(PluginProcessor).getCurrentPosition();
    beats = info.timeSigNumerator;
  #endif

    if (props->tempoSync) {
        lengthSecs = beats / props->getEffectiveScale();
    } else if (props->scale != 1) {
        lengthSecs *= props->getEffectiveScale();
    }

    scalesImage = Image(Image::ARGB, 256 * textTextureScale, 16 * textTextureScale, true);
    Graphics g(scalesImage);
    g.addTransform(AffineTransform::scale((float) textTextureScale));
    g.setFont(font);

    for (float vertMajorLine : vertMajorLines) {
        float seconds = vertMajorLine * lengthSecs;
        String text;

        int decimals = jlimit(1, 4, int(-logf(seconds)));

        if(seconds == 0) {
            text = String("0");
        } else if(seconds < 1.f) {
            text = String(seconds, decimals + 1);
        } else if(seconds < 2.5f) {
            text = String(seconds, decimals);
        } else {
            text = String(roundToInt(seconds));
        }

        int oldPos = position;
        int width = Util::getStringWidth(font, text);

        MiscGraphics::drawShadowedText(g, text, position, font.getHeight(), font, alpha);
        newScales.emplace_back(position, 0, width, font.getHeight());

        position += width + 2;
    }

    ScopedLock sl(renderLock);
    scales = newScales;
}

void Envelope2D::drawScales()
{
    gfx->setCurrentColour(Color(1));
    float fontOffset = 3 - getSetting(PointSizeScale) * 1.5f;

    for (int i = 0; i < (int) scales.size(); ++i) {
        Rectangle<float>& rect = scales[i];
        float x = vertMajorLines[i];

        scalesTex->rect = Rectangle<float>(sx(x) + 1, fontOffset, rect.getWidth(), rect.getHeight());
        gfx->drawSubTexture(scalesTex, rect);
    }
}

void Envelope2D::postCurveDraw() {
    float currentPos = getObj(PlaybackPanel).getEnvelopePos();

    gfx->setCurrentLineWidth(1.f);
    gfx->setCurrentColour(0.3f, 0.3f, 0.3f);
    gfx->setOpacity(1.f);
    gfx->drawLine(currentPos, 0, currentPos, 1, true);
}

juce::Component* Envelope2D::getComponent(int which) {
    switch (which) {
        case CycleTour::TargVol: 		return &e2Interactor->volumeIcon;
        case CycleTour::TargScratch:	return &e2Interactor->scratchIcon;
        case CycleTour::TargPitch:		return &e2Interactor->pitchIcon;
        case CycleTour::TargWavPitch:	return &e2Interactor->wavePitchIcon;
        case CycleTour::TargScratchLyr:	return &e2Interactor->layerSelector;
        case CycleTour::TargSustLoop:	return loopCO.get();
        default: break;
    }

    return nullptr;
}

// EnvRasterizer::LayerProps& Envelope2D::getScratchProps(int index) {
//     int scratchEnv = index;
//
//     if (index < 0) {
//         scratchEnv = getObj(MeshLibrary).getCurrentIndex(LayerGroups::GroupScratch);
//     }
//
//     if (isPositiveAndBelow(scratchEnv, scratchProps.size()))
//         return scratchProps.getReference(scratchEnv);
//
//     return defaultProps;
// }
//
// void Envelope2D::removeScratchProps(int index) {
//     if (isPositiveAndBelow(index, scratchProps.size())) {
//         scratchProps.remove(index);
//
//         if (scratchProps.size() == 1) {
//             scratchProps.add(defaultProps);
//         }
//     }
// }

bool Envelope2D::readXML(const XmlElement* element) {
    XmlElement* envProps = element->getChildByName("EnvelopeProps");

    if (envProps == nullptr) {
        return false;
    }

    auto& meshLibrary = getObj(MeshLibrary);

    for (const auto& mapping : envelopeMappings) {
        MeshLibrary::LayerGroup& group = meshLibrary.getLayerGroup(mapping.groupId);

        group.layers.clear();

        jassert(envProps != nullptr);

        for (auto elem : envProps->getChildWithTagNameIterator(mapping.xmlTag)) {
            if (elem == nullptr) {
                continue;
            }

            MeshLibrary::Layer layer = meshLibrary.instantiateLayer(elem, group.meshType);
            group.layers.push_back(layer);
        }

        if (group.layers.empty()) {
            group.layers.push_back(meshLibrary.instantiateLayer(nullptr, group.meshType));
        }

        group.current = jlimit(0, jmax(0, group.size() - 1), envProps->getIntAttribute(mapping.currentIndexAttr, 0));
    }

    getSetting(CurrentEnvGroup) = envProps->getIntAttribute("currentEnvGroup", getSetting(CurrentEnvGroup));
    e2Interactor->enablementsChanged();
    e2Interactor->switchedEnvelope(getSetting(CurrentEnvGroup), false, true);

    return true;
}

void Envelope2D::writeXML(XmlElement* element) const {
    auto* envProps = new XmlElement("EnvelopeProps");
    auto& meshLibrary = const_cast<MeshLibrary&>(getObj(MeshLibrary));

    envProps->setAttribute("currentEnvGroup", getSettingValue(CurrentEnvGroup));

    for (const auto& mapping : envelopeMappings) {
        const auto& group = meshLibrary.getLayerGroup(mapping.groupId);
        envProps->setAttribute(mapping.currentIndexAttr, group.current);

        for (const auto& layer : group.layers) {
            auto* layerElem = new XmlElement(mapping.xmlTag);

            layer.mesh->writeXML(layerElem);

            if (layer.props != nullptr) {
                layer.props->writeXML(layerElem);
            }

            envProps->addChildElement(layerElem);
        }
    }

    element->addChildElement(envProps);
}

var Envelope2D::writeJSON() const {
    auto json = PresetJson::object();
    auto groups = PresetJson::object();
    auto& meshLibrary = const_cast<MeshLibrary&>(getObj(MeshLibrary));

    json->setProperty("currentGroup", getSettingValue(CurrentEnvGroup));

    for (const auto& mapping : envelopeMappings) {
        groups->setProperty(mapping.jsonKey, writeEnvelopeGroupJson(meshLibrary, mapping));
    }

    json->setProperty("groups", PresetJson::toVar(groups));
    return PresetJson::toVar(json);
}

bool Envelope2D::readJSON(const var& object) {
    auto* json = PresetJson::getObject(object);

    if (json == nullptr) {
        return false;
    }

    auto* groupsJson = PresetJson::getObject(PresetJson::property(object, "groups"));

    if (groupsJson == nullptr) {
        return false;
    }

    auto& meshLibrary = getObj(MeshLibrary);

    // TODO: Envelope meshes are currently rebuilt here after MeshLibrary has already
    // restored its own copy from the preset. Unify this so there is a single
    // authoritative env-mesh restore path and rasterizers only need rebinding.
    // Update: might be done after merge
    for (const auto& mapping : envelopeMappings) {
        var groupValue = groupsJson->getProperty(mapping.jsonKey);
        auto* groupObject = PresetJson::getObject(groupValue);

        if (groupObject == nullptr) {
            continue;
        }

        MeshLibrary::LayerGroup& group = meshLibrary.getLayerGroup(mapping.groupId);
        for (auto& layer : group.layers) {
            meshLibrary.destroyLayer(layer, false);
        }

        group.layers.clear();

        if (const auto* layers = PresetJson::getArray(groupObject->getProperty("layers"))) {
            for (const auto& layerValue : *layers) {
                MeshLibrary::Layer layer = meshLibrary.instantiateLayer(nullptr, group.meshType);

                if (var meshValue = PresetJson::property(layerValue, "mesh"); !meshValue.isVoid()) {
                    (void) layer.mesh->readJSON(meshValue);
                }

                if (layer.props != nullptr) {
                    if (var propsValue = PresetJson::property(layerValue, "properties"); !propsValue.isVoid()) {
                        (void) layer.props->readJSON(propsValue);
                    }
                }

                group.layers.push_back(layer);
            }
        }

        if (group.layers.empty()) {
            group.layers.push_back(meshLibrary.instantiateLayer(nullptr, group.meshType));
        }

        group.current = jlimit(0, jmax(0, group.size() - 1),
                               PresetJson::intProperty(groupValue, "currentLayer", group.current));
    }

    // TODO re-evaluate this after mesh-listener cleanup merge
    auto bindEnvRasterizer = [this, &meshLibrary](int groupId, const char* label) {
        auto* rast = e2Interactor->getRast(groupId);
        auto* mesh = meshLibrary.getCurrentEnvMesh(groupId);

        DBG(String::formatted("Envelope2D::readJSON rebinding %s rast=%p mesh=%p verts=%d cubes=%d",
                              label,
                              rast,
                              mesh,
                              mesh != nullptr ? mesh->getNumVerts() : -1,
                              mesh != nullptr ? mesh->getNumCubes() : -1));

        jassert(rast != nullptr);
        rast->setMesh(mesh);
        rast->performUpdate(Update);
    };
    //
    bindEnvRasterizer(LayerGroups::GroupVolume, "volume");
    bindEnvRasterizer(LayerGroups::GroupPitch, "pitch");
    bindEnvRasterizer(LayerGroups::GroupScratch, "scratch");

    getSetting(CurrentEnvGroup) = PresetJson::intProperty(object, "currentGroup", getSetting(CurrentEnvGroup));
    e2Interactor->enablementsChanged();
    e2Interactor->switchedEnvelope(getSetting(CurrentEnvGroup), false, true);
    return true;
}
