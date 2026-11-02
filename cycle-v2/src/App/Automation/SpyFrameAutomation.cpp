#include <UI/Panels/TimeSurfaceStyles.h>

#include "App/Automation/SpyFrameAutomation.h"
#include "App/Automation/CycleV2AutomationProtocol.h"

namespace CycleV2 {

namespace {

bool readInteger(const var& value, int minimum, int maximum, int& result) {
    if (!value.isInt() && !value.isInt64()) {
        return false;
    }
    const int64 number = value;
    if (number < minimum || number > maximum) {
        return false;
    }
    result = (int) number;
    return true;
}

const TimeSurfaceStyles::Entry* gradientNamed(const String& name) {
    for (const auto& entry : TimeSurfaceStyles::entries) {
        if (name.equalsIgnoreCase(entry.id) || name.equalsIgnoreCase(entry.label)) {
            return &entry;
        }
    }
    return nullptr;
}

}

var SpyFrameAutomation::gradients() {
    Array<var> entries;
    for (const auto& entry : TimeSurfaceStyles::entries) {
        auto object = std::make_unique<DynamicObject>();
        object->setProperty("id", entry.id);
        object->setProperty("label", entry.label);
        const auto group = entry.group == TimeSurfaceStyles::Group::Canonical ? "canonical"
                : entry.group == TimeSurfaceStyles::Group::Experimental ? "experimental" : "legacy";
        object->setProperty("group", group);
        entries.add(var(object.release()));
    }
    return AutomationProtocol::okResult("listSpyGradients", entries);
}

std::optional<SpyFrameRequest> SpyFrameAutomation::readRequest(const var& command, String& error) {
    SpyFrameRequest request;
    if (const auto value = command["probeId"]; !value.isVoid()) {
        if (!value.isString() || value.toString().isEmpty()) {
            error = "Expected a nonempty probeId";
            return std::nullopt;
        }
        request.probeId = value.toString();
    }
    if ((!command["width"].isVoid() && !readInteger(command["width"], 1, 16384, request.width))
            || (!command["height"].isVoid() && !readInteger(command["height"], 1, 16384, request.height))) {
        error = "Image dimensions must be integers in [1, 16384]";
        return std::nullopt;
    }
    if ((int64) request.width * request.height > 64 * 1024 * 1024) {
        error = "Image dimensions exceed 64 megapixels";
        return std::nullopt;
    }
    if (!command["midiNote"].isVoid()) {
        int note {};
        if (!readInteger(command["midiNote"], 0, 127, note)) {
            error = "midiNote must be an integer in [0, 127]";
            return std::nullopt;
        }
        request.midiNote = note;
    }
    if (const auto value = command["gradient"]; !value.isVoid()) {
        const auto* entry = value.isString() ? gradientNamed(value.toString()) : nullptr;
        if (entry == nullptr) {
            StringArray choices;
            for (const auto& choice : TimeSurfaceStyles::entries) {
                choices.add(choice.id);
            }
            error = "Unknown gradient; available: " + choices.joinIntoString(", ");
            return std::nullopt;
        }
        request.gradient = entry->style;
    }
    const auto values = command["controllers"];
    const auto* controllers = values.getArray();
    if (controllers == nullptr || controllers->size() != 128) {
        error = "controllers must contain exactly 128 integer values in [0, 127]";
        return std::nullopt;
    }
    for (int index = 0; index < controllers->size(); ++index) {
        if (!readInteger((*controllers)[index], 0, 127, request.controllers[(size_t) index])) {
            error = "controllers must contain exactly 128 integer values in [0, 127]";
            return std::nullopt;
        }
    }
    error.clear();
    return request;
}

var SpyFrameAutomation::writeFrame(const SpyFrameResult& frame, const File& destination) {
    if (!frame.image.isValid() || destination == File() || destination.exists()) {
        return AutomationProtocol::failedResult("renderSpyFrame", "Expected a rendered image and a new PNG path");
    }
    if (!destination.getParentDirectory().createDirectory()) {
        return AutomationProtocol::failedResult("renderSpyFrame", "Could not create PNG directory");
    }
    TemporaryFile temporary(destination);
    {
        FileOutputStream stream(temporary.getFile());
        if (!stream.openedOk() || !PNGImageFormat().writeImageToStream(frame.image, stream)) {
            return AutomationProtocol::failedResult("renderSpyFrame", "Could not write PNG: " + destination.getFullPathName());
        }
        stream.flush();
        if (stream.getStatus().failed()) {
            return AutomationProtocol::failedResult("renderSpyFrame", stream.getStatus().getErrorMessage());
        }
    }
    if (!temporary.overwriteTargetFileWithTemporary()) {
        return AutomationProtocol::failedResult("renderSpyFrame", "Could not publish PNG: " + destination.getFullPathName());
    }
    auto data = std::make_unique<DynamicObject>();
    data->setProperty("path", destination.getFullPathName());
    data->setProperty("width", frame.image.getWidth());
    data->setProperty("height", frame.image.getHeight());
    data->setProperty("midiNote", frame.midiNote);
    data->setProperty("gradient", TimeSurfaceStyles::id(frame.gradient));
    data->setProperty("noteOffTime", frame.noteOffTime);
    data->setProperty("contentRevision", String(frame.contentRevision));
    return AutomationProtocol::okResult("renderSpyFrame", var(data.release()));
}

}
