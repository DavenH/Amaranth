#include "UnisonNode.h"

#include "../../Graph/NodeParameterMap.h"

#include "../../Graph/NodeModelDecodeDiagnostics.h"

#include <algorithm>
#include <cmath>

namespace CycleV2 {

namespace {

bool validVoice(const UnisonIndividualVoice& voice) {
    return std::isfinite(voice.detunePosition)
            && std::isfinite(voice.pan)
            && std::isfinite(voice.phaseCycles)
            && voice.detunePosition >= 0.f
            && voice.detunePosition <= 1.f
            && voice.pan >= 0.f
            && voice.pan <= 1.f
            && voice.phaseCycles >= 0.f
            && voice.phaseCycles <= 1.f;
}

}

UnisonNodeModelState::UnisonNodeModelState(
        std::vector<UnisonIndividualVoice> voicesToUse,
        uint64_t revisionToUse) :
        individualVoices(std::move(voicesToUse))
    ,   modelRevision(revisionToUse) {}

std::shared_ptr<const UnisonNodeModelState> UnisonNodeModelState::create(
        std::vector<UnisonIndividualVoice> voices,
        uint64_t revision) {
    if (voices.empty()) {
        voices.push_back({});
    }
    if (voices.size() > (size_t) CycleDsp::maximumUnisonOrder) {
        voices.resize(CycleDsp::maximumUnisonOrder);
    }
    return std::shared_ptr<const UnisonNodeModelState>(
            new UnisonNodeModelState(std::move(voices), revision));
}

String UnisonNodeModelState::schemaId() const {
    return "unisonVoices";
}

int UnisonNodeModelState::schemaVersion() const {
    return 1;
}

uint64_t UnisonNodeModelState::revision() const {
    return modelRevision;
}

var UnisonNodeModelState::writeJSON() const {
    auto result = std::make_unique<DynamicObject>();
    result->setProperty("schema", schemaId());
    result->setProperty("version", schemaVersion());
    result->setProperty("revision", (int64) modelRevision);
    Array<var> voices;
    for (const auto& voice : individualVoices) {
        auto encoded = std::make_unique<DynamicObject>();
        encoded->setProperty("detune", voice.detunePosition);
        encoded->setProperty("pan", voice.pan);
        encoded->setProperty("phase", voice.phaseCycles);
        voices.add(var(encoded.release()));
    }
    result->setProperty("voices", voices);
    return var(result.release());
}

bool UnisonNodeModelState::equals(const NodeModelState& other) const {
    const auto* typed = dynamic_cast<const UnisonNodeModelState*>(&other);
    if (typed == nullptr
            || modelRevision != typed->modelRevision
            || individualVoices.size() != typed->individualVoices.size()) {
        return false;
    }
    for (size_t index = 0; index < individualVoices.size(); ++index) {
        const auto& left = individualVoices[index];
        const auto& right = typed->individualVoices[index];
        if (left.detunePosition != right.detunePosition
                || left.pan != right.pan
                || left.phaseCycles != right.phaseCycles) {
            return false;
        }
    }
    return true;
}

String UnisonNodeModelCodec::schemaId() const {
    return "unisonVoices";
}

int UnisonNodeModelCodec::currentVersion() const {
    return 1;
}

NodeModelStatePtr UnisonNodeModelCodec::createDefault() const {
    return UnisonNodeModelState::create({ {} }, 1);
}

NodeModelStatePtr UnisonNodeModelCodec::readJSON(const var& value, String& error) const {
    NodeModelDecodeDiagnostics::recordDecode();
    const auto* object = value.getDynamicObject();
    if (object == nullptr || object->getProperty("schema").toString() != schemaId()) {
        error = "Expected Unison model schema 'unisonVoices'";
        return nullptr;
    }
    if ((int) object->getProperty("version") != currentVersion()) {
        error = "Unsupported Unison model schema version";
        return nullptr;
    }
    const int64 revision = object->getProperty("revision");
    const auto* encodedVoices = object->getProperty("voices").getArray();
    if (revision < 1 || encodedVoices == nullptr || encodedVoices->isEmpty()
            || encodedVoices->size() > CycleDsp::maximumUnisonOrder) {
        error = "Unison voice state is incomplete";
        return nullptr;
    }
    std::vector<UnisonIndividualVoice> voices;
    voices.reserve((size_t) encodedVoices->size());
    for (const auto& encoded : *encodedVoices) {
        const auto* voiceObject = encoded.getDynamicObject();
        if (voiceObject == nullptr) {
            error = "Invalid Unison voice state";
            return nullptr;
        }
        UnisonIndividualVoice voice {
                (float) voiceObject->getProperty("detune"),
                (float) voiceObject->getProperty("pan"),
                (float) voiceObject->getProperty("phase")
        };
        if (!validVoice(voice)) {
            error = "Unison voice values must be finite and normalized";
            return nullptr;
        }
        voices.push_back(voice);
    }
    return UnisonNodeModelState::create(std::move(voices), (uint64_t) revision);
}

std::shared_ptr<const UnisonNodeConfiguration> buildUnisonNodeConfiguration(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model) {
    auto configuration = std::make_shared<UnisonNodeConfiguration>();
    const NodeParameterMap parameterMap(parameters);
    configuration->group.enabled = parameterMap.boolValue("enabled", true);
    configuration->group.order = parameterMap.intValue("order", 1);
    configuration->group.detuneWidthCents = parameterMap.floatValue(
            "width",
            CycleDsp::maximumUnisonDetuneCents * 0.5f);
    configuration->group.panSpread = parameterMap.floatValue("panSpread", 1.f);
    configuration->group.phaseSpread = parameterMap.floatValue("phase", 0.5f);
    configuration->group.jitter = parameterMap.floatValue("jitter", 0.5f);
    configuration->individualMode = parameterMap.stringValue("mode", "group")
            == "individual";
    if (configuration->individualMode) {
        CycleDsp::UnisonIndividualConfiguration individual;
        individual.enabled = true;
        individual.detuneWidthCents = configuration->group.detuneWidthCents;
        const auto typedModel = std::dynamic_pointer_cast<const UnisonNodeModelState>(model);
        const auto voices = typedModel != nullptr
                ? typedModel->voices()
                : std::vector<UnisonIndividualVoice> { {} };
        individual.order = (int) voices.size();
        for (int index = 0; index < individual.order; ++index) {
            individual.detunePositions[(size_t) index] = voices[(size_t) index].detunePosition;
            individual.pans[(size_t) index] = voices[(size_t) index].pan;
            individual.phaseCycles[(size_t) index] = voices[(size_t) index].phaseCycles;
        }
        configuration->layout = CycleDsp::UnisonCore::makeIndividualLayout(individual);
    } else {
        auto layoutConfiguration = configuration->group;
        layoutConfiguration.enabled = true;
        configuration->layout = CycleDsp::UnisonCore::makeGroupLayout(layoutConfiguration);
    }
    return configuration;
}

}
