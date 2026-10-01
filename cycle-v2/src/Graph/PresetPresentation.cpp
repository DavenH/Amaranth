#include <cmath>

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

namespace {

constexpr int kPresentationVersion = 1;
constexpr int kMaximumPreviewWidth = 1024;
constexpr int kMaximumPreviewHeight = 1024;
constexpr size_t kMaximumPreviewBytes = 512 * 1024;

std::optional<PresetMidiSequence> readSequence(const juce::var& value) {
    const auto* object = value.getDynamicObject();
    if (object == nullptr) {
        return std::nullopt;
    }
    const double duration = (double) object->getProperty("durationSeconds");
    if (!std::isfinite(duration) || duration <= 0.0
            || duration > PresetMidiSequence::maximumDurationSeconds) {
        return std::nullopt;
    }
    PresetMidiSequence sequence;
    sequence.durationSeconds = duration;
    const juce::var notesValue = object->getProperty("notes");
    if (const auto* notes = notesValue.getArray()) {
        if (notes->size() > (int) PresetMidiSequence::maximumEventsPerLane) {
            return std::nullopt;
        }
        for (const auto& item : *notes) {
            const auto* note = item.getDynamicObject();
            if (note == nullptr) {
                return std::nullopt;
            }
            PresetMidiNote decoded;
            decoded.pitch = (int) note->getProperty("pitch");
            decoded.velocity = (int) note->getProperty("velocity");
            decoded.startSeconds = (double) note->getProperty("startSeconds");
            decoded.durationSeconds = (double) note->getProperty("durationSeconds");
            if (decoded.pitch < 0 || decoded.pitch > 127
                    || decoded.velocity < 1 || decoded.velocity > 127
                    || !std::isfinite(decoded.startSeconds)
                    || !std::isfinite(decoded.durationSeconds)
                    || decoded.startSeconds < 0.0 || decoded.durationSeconds <= 0.0
                    || decoded.startSeconds + decoded.durationSeconds > duration + 0.001) {
                return std::nullopt;
            }
            sequence.notes.push_back(decoded);
        }
    }
    const juce::var controlsValue = object->getProperty("controls");
    if (const auto* controls = controlsValue.getArray()) {
        if (controls->size() > (int) PresetMidiSequence::maximumEventsPerLane) {
            return std::nullopt;
        }
        for (const auto& item : *controls) {
            const auto* control = item.getDynamicObject();
            if (control == nullptr) {
                return std::nullopt;
            }
            PresetMidiControl decoded;
            decoded.controller = (int) control->getProperty("controller");
            decoded.value = (int) control->getProperty("value");
            decoded.timeSeconds = (double) control->getProperty("timeSeconds");
            if (decoded.controller < 0 || decoded.controller > 127
                    || decoded.value < 0 || decoded.value > 127
                    || !std::isfinite(decoded.timeSeconds)
                    || decoded.timeSeconds < 0.0 || decoded.timeSeconds > duration) {
                return std::nullopt;
            }
            sequence.controls.push_back(decoded);
        }
    }
    return sequence;
}

juce::var writeSequence(const PresetMidiSequence& sequence) {
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("durationSeconds", sequence.durationSeconds);
    juce::Array<juce::var> notes;
    for (const auto& note : sequence.notes) {
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty("pitch", note.pitch);
        item->setProperty("velocity", note.velocity);
        item->setProperty("startSeconds", note.startSeconds);
        item->setProperty("durationSeconds", note.durationSeconds);
        notes.add(juce::var(item.release()));
    }
    result->setProperty("notes", std::move(notes));
    juce::Array<juce::var> controls;
    for (const auto& control : sequence.controls) {
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty("controller", control.controller);
        item->setProperty("value", control.value);
        item->setProperty("timeSeconds", control.timeSeconds);
        controls.add(juce::var(item.release()));
    }
    result->setProperty("controls", std::move(controls));
    return juce::var(result.release());
}

bool isValidDimension(int value, int maximum) {
    return value > 0 && value <= maximum;
}

std::optional<PresetPreviewImage> readPreview(const juce::var& value, juce::String& warning) {
    const auto* object = value.getDynamicObject();
    if (object == nullptr
            || object->getProperty("mediaType").toString() != "image/jpeg") {
        warning = "Preset preview must be a JPEG object";
        return std::nullopt;
    }

    PresetPreviewImage preview;
    preview.width = (int) object->getProperty("width");
    preview.height = (int) object->getProperty("height");
    const auto view = presetPreviewViewForId(object->getProperty("view").toString());
    if (!isValidDimension(preview.width, kMaximumPreviewWidth)
            || !isValidDimension(preview.height, kMaximumPreviewHeight)
            || !view.has_value()) {
        warning = "Preset preview dimensions or view are invalid";
        return std::nullopt;
    }
    preview.view = *view;

    juce::MemoryOutputStream decoded;
    const juce::String data = object->getProperty("data").toString();
    if (data.isEmpty() || !juce::Base64::convertFromBase64(decoded, data)
            || decoded.getDataSize() > kMaximumPreviewBytes) {
        warning = "Preset preview data is invalid or too large";
        return std::nullopt;
    }
    preview.jpegData = decoded.getMemoryBlock();

    const juce::Image image = juce::ImageFileFormat::loadFrom(preview.jpegData.getData(),
            preview.jpegData.getSize());
    if (!image.isValid()
            || image.getWidth() != preview.width
            || image.getHeight() != preview.height) {
        warning = "Preset preview JPEG does not match its declared dimensions";
        return std::nullopt;
    }
    return preview;
}

void readTags(
        const juce::var& value,
        PresetPresentationDecodeResult& result) {
    if (const auto* tags = value.getArray()) {
        for (const auto& tag : *tags) {
            if (tag.isString() && tag.toString().isNotEmpty()) {
                result.presentation.tags.addIfNotAlreadyThere(tag.toString());
            }
        }
    } else if (!value.isVoid()) {
        result.warning = "Preset presentation tags must be an array";
    }
}

PresetPresentationDecodeResult readPresentation(
        const juce::var& value,
        bool includePreview) {
    PresetPresentationDecodeResult result;
    if (value.isVoid()) {
        return result;
    }

    const auto* object = value.getDynamicObject();
    if (object == nullptr || (int) object->getProperty("version") != kPresentationVersion) {
        result.warning = "Preset presentation has an unsupported schema";
        return result;
    }

    result.presentation.author = object->getProperty("author").toString();
    result.presentation.pack = object->getProperty("pack").toString();
    result.presentation.description = object->getProperty("description").toString();
    result.presentation.timeSurfaceStyle = object->getProperty("timeSurfaceStyle").toString();
    result.presentation.patternId = object->getProperty("patternId").toString();
    result.presentation.rating = juce::jlimit(0, 5, (int) object->getProperty("rating"));

    readTags(object->getProperty("tags"), result);

    const juce::var previewValue = object->getProperty("preview");
    if (includePreview && !previewValue.isVoid()) {
        result.presentation.preview = readPreview(previewValue, result.warning);
    }
    const juce::var sequenceValue = object->getProperty("sequence");
    if (!sequenceValue.isVoid()) {
        result.presentation.sequence = readSequence(sequenceValue);
        if (!result.presentation.sequence.has_value()) {
            result.warning = "Preset MIDI sequence is invalid";
        }
    }
    return result;
}

}

bool PresetPreviewImage::isValid() const {
    return width > 0 && height > 0 && !jpegData.isEmpty();
}

bool PresetPresentation::empty() const {
    return author.isEmpty()
            && pack.isEmpty()
            && description.isEmpty()
            && timeSurfaceStyle.isEmpty()
            && tags.isEmpty()
            && rating == 0
            && !preview.has_value()
            && patternId.isEmpty()
            && !sequence.has_value();
}

juce::var PresetPresentationCodec::writeJSON(const PresetPresentation& presentation) {
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("version", kPresentationVersion);
    if (presentation.timeSurfaceStyle.isNotEmpty()) {
        result->setProperty("timeSurfaceStyle", presentation.timeSurfaceStyle);
    }
    if (presentation.author.isNotEmpty()) {
        result->setProperty("author", presentation.author);
    }
    if (presentation.pack.isNotEmpty()) {
        result->setProperty("pack", presentation.pack);
    }
    if (presentation.description.isNotEmpty()) {
        result->setProperty("description", presentation.description);
    }
    if (!presentation.tags.isEmpty()) {
        juce::Array<juce::var> tags;
        for (const auto& tag : presentation.tags) {
            tags.add(tag);
        }
        result->setProperty("tags", std::move(tags));
    }
    if (presentation.rating > 0) {
        result->setProperty("rating", juce::jlimit(0, 5, presentation.rating));
    }
    if (presentation.preview.has_value() && presentation.preview->isValid()) {
        const PresetPreviewImage& preview = *presentation.preview;
        auto encoded = std::make_unique<juce::DynamicObject>();
        encoded->setProperty("mediaType", "image/jpeg");
        encoded->setProperty("width", preview.width);
        encoded->setProperty("height", preview.height);
        encoded->setProperty("view", idForPresetPreviewView(preview.view));
        encoded->setProperty("data", juce::Base64::toBase64(
                preview.jpegData.getData(), preview.jpegData.getSize()));
        result->setProperty("preview", juce::var(encoded.release()));
    }
    if (presentation.patternId.isNotEmpty()) {
        result->setProperty("patternId", presentation.patternId);
    } else if (presentation.sequence.has_value()) {
        result->setProperty("sequence", writeSequence(*presentation.sequence));
    }
    return juce::var(result.release());
}

PresetPresentationDecodeResult PresetPresentationCodec::readJSON(const juce::var& value) {
    return readPresentation(value, true);
}

PresetPresentationDecodeResult PresetPresentationCodec::readMetadataJSON(
        const juce::var& value) {
    return readPresentation(value, false);
}

juce::var PresetPresentationCodec::writeSequenceJSON(
        const PresetMidiSequence& sequence) {
    return writeSequence(sequence);
}

std::optional<PresetMidiSequence> PresetPresentationCodec::readSequenceJSON(
        const juce::var& value) {
    return readSequence(value);
}

juce::String idForPresetPreviewView(PresetPreviewView view) {
    return view == PresetPreviewView::Time ? "time" : "spectrum";
}

std::optional<PresetPreviewView> presetPreviewViewForId(const juce::String& id) {
    if (id == "time") {
        return PresetPreviewView::Time;
    }
    if (id == "spectrum") {
        return PresetPreviewView::Spectrum;
    }
    return std::nullopt;
}

}
