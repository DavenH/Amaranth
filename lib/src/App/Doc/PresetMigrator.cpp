#include "PresetMigrator.h"

#include "PresetJson.h"
#include "../MeshLibrary.h"

namespace {
    using namespace juce;

    enum CurrentGroupIndex {
        GroupVolumeCurrent = LayerGroups::GroupVolume,
        GroupPitchCurrent = LayerGroups::GroupPitch,
        GroupScratchCurrent = LayerGroups::GroupScratch,
        GroupGuideCurveCurrent = LayerGroups::GroupGuideCurve,
        GroupTimeCurrent = LayerGroups::GroupTime,
        GroupSpectCurrent = LayerGroups::GroupSpect,
        GroupPhaseCurrent = LayerGroups::GroupPhase,
        GroupWavePitchCurrent = LayerGroups::numDefaultGroups,
        GroupWaveshaperCurrent,
        GroupIrModellerCurrent
    };

    enum LegacyMeshSection {
        LegacyTimeLayer,
        LegacyFreqLayer,
        LegacyPhaseLayer,
        LegacyGuideLayer,
        LegacyEnvLayer,
        LegacyWaveShaperLayer,
        LegacyTubeModelLayer
    };

    Identifier formatKey("format");
    Identifier schemaVersionKey("schemaVersion");
    Identifier presetKey("preset");

    String getAttributeString(const XmlElement* element, const String& name, const String& fallback = {}) {
        return element == nullptr ? fallback : element->getStringAttribute(name, fallback);
    }

    int getAttributeInt(const XmlElement* element, const String& name, int fallback = 0) {
        return element == nullptr ? fallback : element->getIntAttribute(name, fallback);
    }

    double getAttributeDouble(const XmlElement* element, const String& name, double fallback = 0.0) {
        return element == nullptr ? fallback : element->getDoubleAttribute(name, fallback);
    }

    bool getAttributeBool(const XmlElement* element, const String& name, bool fallback = false) {
        return element == nullptr ? fallback : element->getBoolAttribute(name, fallback);
    }

    XmlElement* getRequiredChild(XmlElement* parent, const String& name) {
        return parent == nullptr ? nullptr : parent->getChildByName(name);
    }

    var parseKnobs(const XmlElement* parent) {
        Array<var> knobs;
        XmlElement* knobsElem = parent == nullptr ? nullptr : parent->getChildByName("Knobs");

        if (knobsElem == nullptr) {
            return var(knobs);
        }

        for (auto knobElem : knobsElem->getChildWithTagNameIterator("Knob")) {
            int number = knobElem->getIntAttribute("number", -1);

            if (number < 0) {
                continue;
            }

            while (knobs.size() <= number) {
                knobs.add(var());
            }

            knobs.set(number, knobElem->getDoubleAttribute("value", 0.0));
        }

        for (int i = 0; i < knobs.size(); ++i) {
            if (knobs.getReference(i).isVoid()) {
                knobs.getReference(i) = 0.0;
            }
        }

        return var(knobs);
    }

    var parseMesh(const XmlElement* meshContainer) {
        auto json = PresetJson::object();
        const XmlElement* meshElem = meshContainer == nullptr ? nullptr : meshContainer->getChildByName("Mesh");
        Array<var> vertices, cubes;

        if (meshElem == nullptr) {
            return {};
        }

        json->setProperty("name", meshElem->getStringAttribute("name", "unnamed"));
        json->setProperty("version", meshElem->getIntAttribute("version", 1));

        for (auto vertexElem : meshElem->getChildWithTagNameIterator("Vertex")) {
            auto vertex = PresetJson::object();

            vertex->setProperty("time", vertexElem->getDoubleAttribute("time"));
            vertex->setProperty("phase", vertexElem->getDoubleAttribute("phase"));
            vertex->setProperty("amp", vertexElem->getDoubleAttribute("amp"));
            vertex->setProperty("key", vertexElem->getDoubleAttribute("key"));
            vertex->setProperty("mod", vertexElem->getDoubleAttribute("mod"));
            vertex->setProperty("weight", vertexElem->getDoubleAttribute("weight"));
            vertex->setProperty("id", vertexElem->getIntAttribute("id"));
            vertices.add(PresetJson::toVar(vertex));
        }

        for (auto cubeElem : meshElem->getChildWithTagNameIterator("VertCube")) {
            auto cube = PresetJson::object();
            auto guides = PresetJson::object();
            auto gains = PresetJson::object();
            Array<var> vertexIds;

            for (auto cubeVertexElem : cubeElem->getChildWithTagNameIterator("Vertex")) {
                int lineVertexNumber = cubeVertexElem->getIntAttribute("lineVertexNumber", -1);

                if (lineVertexNumber < 0) {
                    continue;
                }

                while (vertexIds.size() <= lineVertexNumber) {
                    vertexIds.add(-1);
                }

                vertexIds.set(lineVertexNumber, cubeVertexElem->getIntAttribute("vertexId", -1));
            }

            guides->setProperty("time", cubeElem->getIntAttribute("avpGuide", cubeElem->getIntAttribute("timeGuide", -1)));
            guides->setProperty("key", cubeElem->getIntAttribute("keyGuide", -1));
            guides->setProperty("mod", cubeElem->getIntAttribute("modGuide", -1));
            guides->setProperty("phase", cubeElem->getIntAttribute("phaseGuide", -1));
            guides->setProperty("amp", cubeElem->getIntAttribute("ampGuide", -1));
            guides->setProperty("curve", cubeElem->getIntAttribute("curveGuide", -1));

            gains->setProperty("time", cubeElem->getDoubleAttribute("avpGain", 0.5));
            gains->setProperty("key", cubeElem->getDoubleAttribute("keyGain", 0.5));
            gains->setProperty("mod", cubeElem->getDoubleAttribute("modGain", 0.5));
            gains->setProperty("phase", cubeElem->getDoubleAttribute("phaseGain", 0.5));
            gains->setProperty("amp", cubeElem->getDoubleAttribute("ampGain", 0.5));
            gains->setProperty("curve", cubeElem->getDoubleAttribute("curveGain", 0.5));

            cube->setProperty("vertexIds", var(vertexIds));
            cube->setProperty("guides", PresetJson::toVar(guides));
            cube->setProperty("gains", PresetJson::toVar(gains));
            cubes.add(PresetJson::toVar(cube));
        }

        json->setProperty("vertices", var(vertices));
        json->setProperty("cubes", var(cubes));

        return PresetJson::toVar(json);
    }

    var parseEnvelopeMesh(const XmlElement* wrapper) {
        auto json = PresetJson::object();
        XmlElement* envMeshElem = wrapper == nullptr ? nullptr : wrapper->getChildByName("EnvelopeMesh");
        Array<var> loopIndices, sustainIndices;

        if (envMeshElem == nullptr) {
            return {};
        }

        json->setProperty("name", envMeshElem->getStringAttribute("name", "Mesh"));
        json->setProperty("mainMesh", parseMesh(envMeshElem->getChildByName("MainMesh")));

        if (XmlElement* loopIndicesElem = envMeshElem->getChildByName("LoopIndices")) {
            for (auto loopElem : loopIndicesElem->getChildWithTagNameIterator("LoopIndex")) {
                loopIndices.add(loopElem->getIntAttribute("index", -1));
            }
        } else {
            int sustainIndex = envMeshElem->getIntAttribute("sustainIndex", -1);
            if (sustainIndex >= 0) {
                loopIndices.add(sustainIndex);
            }
        }

        if (XmlElement* sustainIndicesElem = envMeshElem->getChildByName("SustIndices")) {
            for (auto sustainElem : sustainIndicesElem->getChildWithTagNameIterator("SustIndex")) {
                sustainIndices.add(sustainElem->getIntAttribute("index", -1));
            }
        } else {
            int sustainIndex = envMeshElem->getIntAttribute("sustainIndex2", -1);
            if (sustainIndex >= 0) {
                sustainIndices.add(sustainIndex);
            }
        }

        json->setProperty("loopIndices", var(loopIndices));
        json->setProperty("sustainIndices", var(sustainIndices));

        return PresetJson::toVar(json);
    }

    var parseLayerProperties(const XmlElement* layerElem, bool isEnvelope) {
        auto json = PresetJson::object();

        json->setProperty("active", getAttributeBool(layerElem, "active", true));
        json->setProperty("gain", getAttributeDouble(layerElem, "gain", 0.0));
        json->setProperty("range", getAttributeDouble(layerElem, "range", 0.0));
        json->setProperty("mode", getAttributeInt(layerElem, "mode", 0));
        json->setProperty("scratchChannel", getAttributeInt(layerElem, "scratch-chan", 0));
        json->setProperty("fineTune", getAttributeDouble(layerElem, "fine-tune", 0.0));
        json->setProperty("pan", getAttributeDouble(layerElem, "pan", 0.0));

        if (isEnvelope) {
            json->setProperty("dynamic", getAttributeBool(layerElem, "dynamic", false));
            json->setProperty("tempoSync", getAttributeBool(layerElem, "tempo-sync", false));
            json->setProperty("global", getAttributeBool(layerElem, "global", false));
            json->setProperty("logarithmic", getAttributeBool(layerElem, "logarithmic", false));
            json->setProperty("scale", getAttributeInt(layerElem, "scale", 1));
        }

        return PresetJson::toVar(json);
    }

    var parseCurrentMeshLibrary(XmlElement* repoElem) {
        auto json = PresetJson::object();
        Array<var> groups;

        if (repoElem == nullptr) {
            return {};
        }

        for (auto groupElem : repoElem->getChildWithTagNameIterator("Group")) {
            auto group = PresetJson::object();
            Array<var> layers;
            int meshType = groupElem->getIntAttribute("mesh-type", MeshLibrary::TypeMesh);

            group->setProperty("meshType", meshType);

            for (auto layerElem : groupElem->getChildWithTagNameIterator("Layer")) {
                auto layer = PresetJson::object();
                bool isEnvelope = meshType == MeshLibrary::TypeEnvelope;

                layer->setProperty("mesh", isEnvelope ? parseEnvelopeMesh(layerElem) : parseMesh(layerElem));
                layer->setProperty("properties", parseLayerProperties(layerElem, isEnvelope));
                layers.add(PresetJson::toVar(layer));
            }

            group->setProperty("layers", var(layers));
            groups.add(PresetJson::toVar(group));
        }

        json->setProperty("groups", var(groups));
        return PresetJson::toVar(json);
    }

    void addLegacyLayer(Array<var>& groups, int groupIndex, int meshType, var mesh, var props) {
        auto group = PresetJson::object();
        Array<var> layers;
        auto layer = PresetJson::object();

        while (groups.size() <= groupIndex) {
            auto emptyGroup = PresetJson::object();
            emptyGroup->setProperty("meshType", MeshLibrary::TypeMesh);
            emptyGroup->setProperty("layers", var(Array<var>()));
            groups.add(PresetJson::toVar(emptyGroup));
        }

        group = PresetJson::getObject(groups.getReference(groupIndex));
        group->setProperty("meshType", meshType);

        if (auto* existingLayers = PresetJson::getArray(group->getProperty("layers"))) {
            layers = *existingLayers;
        }

        layer->setProperty("mesh", mesh);
        layer->setProperty("properties", props);
        layers.add(PresetJson::toVar(layer));
        group->setProperty("layers", var(layers));
    }

    void applyLegacyScalarLayerProps(Array<var>& groups, int groupIndex, const XmlElement* propsElem,
                                     bool isEnabledDefault = true, bool isMagnitude = false) {
        if (!isPositiveAndBelow(groupIndex, groups.size())) {
            return;
        }

        auto* group = PresetJson::getObject(groups.getReference(groupIndex));
        auto* layers = PresetJson::getArray(group->getProperty("layers"));

        if (group == nullptr || layers == nullptr || layers->isEmpty()) {
            return;
        }

        auto layerValue = layers->getReference(0);
        auto* layer = PresetJson::getObject(layerValue);
        auto props = PresetJson::property(layerValue, "properties");
        auto* propsJson = PresetJson::getObject(props);

        if (layer == nullptr || propsJson == nullptr) {
            return;
        }

        propsJson->setProperty("active", getAttributeBool(propsElem, "isEnabled", isEnabledDefault));
        propsJson->setProperty("pan", getAttributeDouble(propsElem, "pan", 0.5));

        if (propsElem->hasAttribute("fine")) {
            propsJson->setProperty("fineTune", getAttributeDouble(propsElem, "fine", 0.0));
        }

        if (propsElem->hasAttribute("dynamicRange")) {
            propsJson->setProperty("range", getAttributeDouble(propsElem, "dynamicRange", 0.0));
        }

        if (isMagnitude && propsElem->hasAttribute("additive")) {
            propsJson->setProperty("mode", getAttributeInt(propsElem, "additive", 0));
        }
    }

    var migrateLegacyAllMeshes(XmlElement* presetElement) {
        auto json = PresetJson::object();
        Array<var> groups;
        XmlElement* allMeshesElem = presetElement == nullptr ? nullptr : presetElement->getChildByName("AllMeshes");

        auto emptyProps = [] (bool isEnvelope) {
            auto props = PresetJson::object();
            props->setProperty("active", true);
            props->setProperty("gain", 0.0);
            props->setProperty("range", 0.0);
            props->setProperty("mode", 0);
            props->setProperty("scratchChannel", 0);
            props->setProperty("fineTune", 0.0);
            props->setProperty("pan", 0.0);

            if (isEnvelope) {
                props->setProperty("dynamic", false);
                props->setProperty("tempoSync", false);
                props->setProperty("global", false);
                props->setProperty("logarithmic", false);
                props->setProperty("scale", 1);
            }

            return PresetJson::toVar(props);
        };

        if (allMeshesElem == nullptr) {
            return {};
        }

        struct LayerMapping {
            const char* xmlName;
            int groupIndex;
            int meshType;
            bool envelope;
        };

        const LayerMapping mappings[] = {
            { "GuideLayer",      LayerGroups::GroupGuideCurve,  MeshLibrary::TypeMesh,     false },
            { "TimeLayer",       LayerGroups::GroupTime,        MeshLibrary::TypeMesh,     false },
            { "FreqLayer",       LayerGroups::GroupSpect,       MeshLibrary::TypeMesh,     false },
            { "PhaseLayer",      LayerGroups::GroupPhase,       MeshLibrary::TypeMesh,     false },
            { "WaveShaperLayer", GroupWaveshaperCurrent,        MeshLibrary::TypeMesh,     false },
            { "TubeModelLayer",  GroupIrModellerCurrent,        MeshLibrary::TypeMesh,     false }
        };

        for (const auto& mapping : mappings) {
            XmlElement* layerElem = allMeshesElem->getChildByName(mapping.xmlName);

            if (layerElem == nullptr) {
                continue;
            }

            for (auto wrapperElem : layerElem->getChildIterator()) {
                if (wrapperElem == nullptr) {
                    continue;
                }

                addLegacyLayer(groups, mapping.groupIndex, mapping.meshType,
                               parseMesh(wrapperElem), emptyProps(mapping.envelope));
            }
        }

        if (XmlElement* envLayer = allMeshesElem->getChildByName("EnvLayer")) {
            struct EnvMapping {
                const char* xmlName;
                int groupIndex;
            };

            const EnvMapping envMappings[] = {
                { "EnvVolumeMesh",    LayerGroups::GroupVolume },
                { "EnvPitchMesh",     LayerGroups::GroupPitch },
                { "EnvSpeedMesh",     LayerGroups::GroupScratch },
                { "EnvWavePitchMesh", GroupWavePitchCurrent }
            };

            for (const auto& envMapping : envMappings) {
                if (XmlElement* wrapperElem = envLayer->getChildByName(envMapping.xmlName)) {
                    var props = emptyProps(true);

                    if (XmlElement* envMesh = wrapperElem->getChildByName("EnvelopeMesh")) {
                        if (auto* propsJson = PresetJson::getObject(props)) {
                            propsJson->setProperty("active", envMesh->getBoolAttribute("primaryEnabled", true));
                        }
                    }

                    addLegacyLayer(groups, envMapping.groupIndex, MeshLibrary::TypeEnvelope,
                                   parseEnvelopeMesh(wrapperElem), props);
                }
            }
        }

        if (XmlElement* timeDomain = presetElement->getChildByName("TimeDomainProperties")) {
            applyLegacyScalarLayerProps(groups, LayerGroups::GroupTime,
                                        timeDomain->getChildByName("TimeProperties"));
        }

        if (XmlElement* freqDomain = presetElement->getChildByName("FreqDomainProperties")) {
            XmlElement* magnitudeProps = nullptr;
            XmlElement* phaseProps = nullptr;

            if (XmlElement* mags = freqDomain->getChildByName("MagsPropertiesSet")) {
                magnitudeProps = mags->getChildByName("MagnitudeProperties");
            }

            if (XmlElement* phase = freqDomain->getChildByName("PhasePropertiesSet")) {
                phaseProps = phase->getChildByName("PhaseProperties");
            }

            applyLegacyScalarLayerProps(groups, LayerGroups::GroupSpect, magnitudeProps, true, true);
            applyLegacyScalarLayerProps(groups, LayerGroups::GroupPhase, phaseProps);
        }

        const int requiredGroups[] = {
            LayerGroups::GroupVolume,
            LayerGroups::GroupPitch,
            LayerGroups::GroupScratch,
            LayerGroups::GroupGuideCurve,
            LayerGroups::GroupTime,
            LayerGroups::GroupSpect,
            LayerGroups::GroupPhase,
            GroupWavePitchCurrent,
            GroupWaveshaperCurrent,
            GroupIrModellerCurrent
        };

        for (int requiredGroup : requiredGroups) {
            while (groups.size() <= requiredGroup) {
                auto group = PresetJson::object();
                group->setProperty("meshType", MeshLibrary::TypeMesh);
                group->setProperty("layers", var(Array<var>()));
                groups.add(PresetJson::toVar(group));
            }

            auto* group = PresetJson::getObject(groups.getReference(requiredGroup));
            auto* layers = PresetJson::getArray(group->getProperty("layers"));

            if (layers != nullptr && !layers->isEmpty()) {
                continue;
            }

            bool isEnvelope = requiredGroup == LayerGroups::GroupVolume ||
                              requiredGroup == LayerGroups::GroupPitch ||
                              requiredGroup == LayerGroups::GroupScratch ||
                              requiredGroup == GroupWavePitchCurrent;

            group->setProperty("meshType", isEnvelope ? MeshLibrary::TypeEnvelope : MeshLibrary::TypeMesh);

            auto layer = PresetJson::object();
            layer->setProperty("mesh", isEnvelope ? parseEnvelopeMesh(nullptr) : parseMesh(nullptr));
            layer->setProperty("properties", emptyProps(isEnvelope));

            Array<var> layersArray;
            layersArray.add(PresetJson::toVar(layer));
            group->setProperty("layers", var(layersArray));
        }

        json->setProperty("groups", var(groups));
        return PresetJson::toVar(json);
    }

    var parseGuideCurves(XmlElement* presetElement) {
        auto json = PresetJson::object();
        Array<var> guides;
        XmlElement* guideElem = presetElement == nullptr ? nullptr : presetElement->getChildByName("GuideCurveProps");

        if (guideElem == nullptr && presetElement != nullptr) {
            guideElem = presetElement->getChildByName("DeformerProps");
        }

        if (guideElem != nullptr) {
            for (auto propsElem : guideElem->getChildWithTagNameIterator("Properties")) {
                auto guide = PresetJson::object();
                guide->setProperty("noiseLevel", propsElem->getDoubleAttribute("noiseLevel", 0));
                guide->setProperty("offsetLevel", propsElem->getDoubleAttribute("offsetLevel", 0));
                guide->setProperty("phaseLevel", propsElem->getDoubleAttribute("phaseLevel", 0));
                guide->setProperty("noiseSeed", propsElem->getIntAttribute("noiseSeed", -1));
                guides.add(PresetJson::toVar(guide));
            }
        }

        json->setProperty("guides", var(guides));
        return PresetJson::toVar(json);
    }

    var parseModMatrix(XmlElement* presetElement) {
        auto json = PresetJson::object();
        Array<var> inputs, outputs, mappings;
        XmlElement* matrixElem = presetElement == nullptr ? nullptr : presetElement->getChildByName("ModMatrix");

        if (matrixElem == nullptr) {
            return {};
        }

        if (XmlElement* inputsElem = matrixElem->getChildByName("Inputs")) {
            for (auto inputElem : inputsElem->getChildWithTagNameIterator("input")) {
                inputs.add(inputElem->getIntAttribute("id", -1));
            }
        }

        if (XmlElement* outputsElem = matrixElem->getChildByName("Outputs")) {
            for (auto outputElem : outputsElem->getChildWithTagNameIterator("output")) {
                outputs.add(outputElem->getIntAttribute("id", -1));
            }
        }

        if (XmlElement* mappingsElem = matrixElem->getChildByName("Mappings")) {
            for (auto mappingElem : mappingsElem->getChildWithTagNameIterator("mapping")) {
                auto mapping = PresetJson::object();
                mapping->setProperty("in", mappingElem->getIntAttribute("in", -1));
                mapping->setProperty("out", mappingElem->getIntAttribute("out", -1));
                mapping->setProperty("dim", mappingElem->getIntAttribute("dim", -1));
                mappings.add(PresetJson::toVar(mapping));
            }
        }

        json->setProperty("inputs", var(inputs));
        json->setProperty("outputs", var(outputs));
        json->setProperty("mappings", var(mappings));
        return PresetJson::toVar(json);
    }

    var parseMainPanel(XmlElement* presetElement) {
        auto json = PresetJson::object();
        auto collapsedView = PresetJson::object();
        auto unifiedView = PresetJson::object();
        XmlElement* mainElem = presetElement == nullptr ? nullptr : presetElement->getChildByName("MainPanel");

        if (mainElem == nullptr) {
            return {};
        }

        collapsedView->setProperty("wholeDragger", mainElem->getDoubleAttribute("CV_WholeDragger"));
        collapsedView->setProperty("middleDragger", mainElem->getDoubleAttribute("CV_MiddleDragger"));
        collapsedView->setProperty("envSpectDragger", mainElem->getDoubleAttribute("CV_EnvSpectDragger"));
        collapsedView->setProperty("spectSurfDragger", mainElem->getDoubleAttribute("CV_SpectSurfDragger"));

        unifiedView->setProperty("wholeDragger", mainElem->getDoubleAttribute("XV_WholeDragger"));
        unifiedView->setProperty("topBottomDragger", mainElem->getDoubleAttribute("XV_TopBttmDragger"));
        unifiedView->setProperty("spectSurfDragger", mainElem->getDoubleAttribute("XV_SpectSurfDragger"));
        unifiedView->setProperty("envDeformerImpulseDragger", mainElem->getDoubleAttribute("XV_EnvDfmImpDragger"));
        unifiedView->setProperty("deformerImpulseDragger", mainElem->getDoubleAttribute("XV_DfrmImpDragger"));

        json->setProperty("collapsedView", PresetJson::toVar(collapsedView));
        json->setProperty("unifiedView", PresetJson::toVar(unifiedView));
        return PresetJson::toVar(json);
    }

    var parseSettings(XmlElement* presetElement) {
        auto json = PresetJson::object();
        XmlElement* settingsElem = presetElement == nullptr ? nullptr : presetElement->getChildByName("Settings");

        if (settingsElem == nullptr) {
            return {};
        }

        for (int i = 0; i < settingsElem->getNumAttributes(); ++i) {
            String name = settingsElem->getAttributeName(i);
            json->setProperty(name, settingsElem->getIntAttribute(name));
        }

        return PresetJson::toVar(json);
    }

    var parseOscControls(XmlElement* presetElement) {
        auto json = PresetJson::object();
        XmlElement* oscElem = presetElement == nullptr ? nullptr : presetElement->getChildByName("OscControls");

        if (oscElem == nullptr) {
            return {};
        }

        json->setProperty("knobs", parseKnobs(oscElem));
        return PresetJson::toVar(json);
    }

    var parseEffect(XmlElement* effectsElem, const String& xmlName, bool addWave = false,
                    bool addOversample = false, bool addVoices = false) {
        XmlElement* effectElem = effectsElem == nullptr ? nullptr : effectsElem->getChildByName(xmlName);

        if (effectElem == nullptr) {
            return {};
        }

        auto json = PresetJson::object();
        json->setProperty("enabled", effectElem->getBoolAttribute("enabled", false));
        json->setProperty("knobs", parseKnobs(effectElem));

        if (addWave) {
            json->setProperty("waveLoaded", effectElem->getBoolAttribute("waveLoaded", false));
            json->setProperty("wavePath", effectElem->getStringAttribute("wavePath"));
        }

        if (addOversample) {
            json->setProperty("oversampleFactor", effectElem->getIntAttribute("oversampleFactor", 1));
        }

        if (addVoices) {
            Array<var> voices;
            json->setProperty("groupMode", effectElem->getBoolAttribute("mode", true));

            if (XmlElement* voiceDataElem = effectElem->getChildByName("VoiceData")) {
                for (auto voiceElem : voiceDataElem->getChildWithTagNameIterator("data")) {
                    auto voice = PresetJson::object();
                    voice->setProperty("fine", voiceElem->getDoubleAttribute("fine", 0.5));
                    voice->setProperty("pan", voiceElem->getDoubleAttribute("pan", 0.5));
                    voice->setProperty("phase", voiceElem->getDoubleAttribute("phase", 0.5));
                    voices.add(PresetJson::toVar(voice));
                }
            }

            json->setProperty("voices", var(voices));
        }

        return PresetJson::toVar(json);
    }

    var parseEffects(XmlElement* presetElement) {
        auto json = PresetJson::object();
        XmlElement* effectsElem = presetElement == nullptr ? nullptr : presetElement->getChildByName("Effects");

        if (effectsElem == nullptr) {
            return {};
        }

        json->setProperty("ImpulseModeller", parseEffect(effectsElem, "ImpulseModeller", true));
        json->setProperty("Unison", parseEffect(effectsElem, "UnisonUI", false, false, true));
        json->setProperty("Waveshaper", parseEffect(effectsElem, "WaveshaperUI", false, true));
        json->setProperty("Delay", parseEffect(effectsElem, "DelayUI"));
        json->setProperty("Reverb", parseEffect(effectsElem, "ReverbUI"));
        json->setProperty("EQ", parseEffect(effectsElem, "EqualizerUI"));
        return PresetJson::toVar(json);
    }

    var parseMorphPanel(XmlElement* presetElement) {
        XmlElement* morphElem = presetElement == nullptr ? nullptr : presetElement->getChildByName("ModMapping");

        if (morphElem == nullptr) {
            return {};
        }

        auto json = PresetJson::object();
        json->setProperty("modMappingId", morphElem->getIntAttribute("modMappingId", 0));
        return PresetJson::toVar(json);
    }
}

var PresetMigrator::migrateV1XmlToCurrentJson(const XmlElement* presetElement, const DocumentDetails& details) {
    auto root = PresetJson::object();
    auto preset = PresetJson::object();

    root->setProperty(formatKey, "amaranth-preset");
    root->setProperty(schemaVersionKey, 2);

    preset->setProperty("details", details.writeJSON());

    if (presetElement != nullptr) {
        if (XmlElement* meshLibraryElem = presetElement->getChildByName("MeshLibrary")) {
            preset->setProperty("meshLibrary", parseCurrentMeshLibrary(meshLibraryElem));
        } else if (presetElement->getChildByName("AllMeshes") != nullptr) {
            preset->setProperty("meshLibrary", migrateLegacyAllMeshes(const_cast<XmlElement*>(presetElement)));
        }

        if (var oscControls = parseOscControls(const_cast<XmlElement*>(presetElement)); !oscControls.isVoid()) {
            preset->setProperty("oscControls", oscControls);
        }

        if (var effects = parseEffects(const_cast<XmlElement*>(presetElement)); !effects.isVoid()) {
            preset->setProperty("effects", effects);
        }

        if (var settings = parseSettings(const_cast<XmlElement*>(presetElement)); !settings.isVoid()) {
            preset->setProperty("settings", settings);
        }

        if (var mainPanel = parseMainPanel(const_cast<XmlElement*>(presetElement)); !mainPanel.isVoid()) {
            preset->setProperty("mainPanel", mainPanel);
        }

        if (var modMatrix = parseModMatrix(const_cast<XmlElement*>(presetElement)); !modMatrix.isVoid()) {
            preset->setProperty("modMatrix", modMatrix);
        }

        if (var guideCurves = parseGuideCurves(const_cast<XmlElement*>(presetElement)); !guideCurves.isVoid()) {
            preset->setProperty("guideCurveProps", guideCurves);
        }

        if (var morphPanel = parseMorphPanel(const_cast<XmlElement*>(presetElement)); !morphPanel.isVoid()) {
            preset->setProperty("morphPanel", morphPanel);
        }
    }

    root->setProperty(presetKey, PresetJson::toVar(preset));
    return PresetJson::toVar(root);
}

var PresetMigrator::migrateXmlToCurrentJson(const XmlElement* presetElement, const DocumentDetails& details) {
    return migrateV1XmlToCurrentJson(presetElement, details);
}
