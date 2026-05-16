#include "CycleAutomation.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

#include <App/AutomationInspectable.h>
#include <App/Doc/Document.h>
#include <App/Doc/PresetJson.h>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <Array/ScopedAlloc.h>
#include <Audio/AudioHub.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Mesh/VertCube.h>
#include <Definitions.h>
#include <Inter/EnvelopeInter2D.h>
#include <Inter/Interactor.h>
#include <UI/Layout/Dragger.h>
#include <UI/Layout/PanelPair.h>
#include <UI/Panels/OpenGLBase.h>
#include <UI/Panels/Panel.h>
#include <UI/Widgets/PulloutComponent.h>
#include <UI/Widgets/RetractableCallout.h>
#include <UI/Widgets/Controls/HoverSelector.h>
#include <UI/Widgets/Controls/SelectorPanel.h>
#include <UI/Widgets/MidiKeyboard.h>
#include <UI/Widgets/TabbedSelector.h>

#include "CycleTour.h"
#include "FileManager.h"
#include "KeyboardInputHandler.h"

#include "../UI/Panels/MainPanel.h"
#include "../UI/Panels/BannerPanel.h"
#include "../UI/Panels/DerivativePanel.h"
#include "../UI/Panels/ModMatrixPanel.h"
#include "../UI/Panels/PlaybackPanel.h"
#include "../UI/Panels/SynthMenuBarModel.h"

#if JUCE_MAC || JUCE_LINUX
  #include <cerrno>
  #include <cstring>
  #include <sys/socket.h>
  #include <sys/un.h>
  #include <unistd.h>
#endif

namespace {
    const char* const kInspectableAreas[] = {
        "AreaMain",
        "AreaSharpBand",
        "AreaWshpEditor",
        "AreaWfrmWaveform3D",
        "AreaSpectrum",
        "AreaSpectrogram",
        "AreaEnvelopes",
        "AreaMorphPanel",
        "AreaVertexProps",
        "AreaGenControls",
        "AreaPlayback",
        "AreaGuideCurves",
        "AreaImpulse",
        "AreaWaveshaper",
        "AreaUnison",
        "AreaReverb",
        "AreaDelay",
        "AreaEQ",
        "AreaModMatrix",
        "AreaMasterCtrls",
    };

    const char* const kTourGuideAreas[] = {
        "AreaWfrmWaveform3D",
        "AreaSpectrogram",
        "AreaEnvelopes",
        "AreaMorphPanel",
        "AreaVertexProps",
        "AreaGenControls",
        "AreaImpulse",
        "AreaWaveshaper",
        "AreaGuideCurves",
        "AreaUnison",
        "AreaReverb",
        "AreaDelay",
        "AreaEQ",
        "AreaModMatrix",
        "AreaMasterCtrls",
    };

    const char* const kInspectableTargets[] = {
        "TargMatrixGrid",
        "TargMatrixUtility",
        "TargMatrixSource",
        "TargMatrixDest",
        "TargMatrixAddDest",
        "TargMatrixAddSource",
        "TargUniMode",
        "TargUniVoiceSlct",
        "TargUniAddRemove",
        "TargDfrmNoise",
        "TargDfrmOffset",
        "TargDfrmPhase",
        "TargDfrmMeshSlct",
        "TargDfrmLayerAdd",
        "TargDfrmLayerSlct",
        "TargImpLength",
        "TargImpGain",
        "TargImpHP",
        "TargImpZoom",
        "TargImpLoadWav",
        "TargImpUnloadWav",
        "TargImpModelWav",
        "TargPlaybackSurface",
        "TargPlaybackZoomAttack",
        "TargPlaybackZoomFull",
        "TargWaveshaperOvsp",
        "TargWaveshaperPre",
        "TargWaveshaperPost",
        "TargWaveshaperSlct",
        "TargSliderArea",
        "TargTimeSlider",
        "TargPhsSlider",
        "TargAmpSlider",
        "TargKeySlider",
        "TargMorphSlider",
        "TargCrvSlider",
        "TargBoxArea",
        "TargPhsBox",
        "TargAmpBox",
        "TargKeyBox",
        "TargModBox",
        "TargCrvBox",
        "TargAvpBox",
        "TargGainArea",
        "TargPhsGain",
        "TargAmpGain",
        "TargKeyGain",
        "TargModGain",
        "TargCrvGain",
        "TargAvpGain",
        "TargVol",
        "TargScratch",
        "TargPitch",
        "TargWavPitch",
        "TargScratchLyr",
        "TargSustLoop",
        "TargDomains",
        "TargLayerEnable",
        "TargLayerMode",
        "TargLayerAdder",
        "TargLayerAddButton",
        "TargLayerRemoveButton",
        "TargLayerMover",
        "TargLayerMoveUpButton",
        "TargLayerMoveDownButton",
        "TargLayerSlct",
        "TargScratchBox",
        "TargDeconv",
        "TargPhaseUp",
        "TargPan",
        "TargRange",
        "TargMeshSelector",
        "TargModelCycle",
        "TargSelector",
        "TargPencil",
        "TargAxe",
        "TargNudge",
        "TargWaveVerts",
        "TargVerts",
        "TargLinkYellow",
        "TargToolPullout",
        "TargPresetPullout",
        "TargTransportPullout",
        "TargWavePullout",
        "TargToolCallout",
        "TargPresetCallout",
        "TargTransportCallout",
        "TargWaveCallout",
        "TargVertCube",
        "TargPrimeArea",
        "TargPrimeY",
        "TargPrimeB",
        "TargPrimeR",
        "TargLinkArea",
        "TargLinkY",
        "TargLinkB",
        "TargLinkR",
        "TargRangeArea",
        "TargRangeY",
        "TargRangeB",
        "TargRangeR",
        "TargSlidersArea",
        "TargSliderY",
        "TargSliderB",
        "TargSliderR",
        "TargSliderPan",
        "TargMasterVol",
        "TargMasterOct",
        "TargMasterLen",
        "TargMainBottomTabs",
        "TargMainTopTabs",
        "TargMidiKeyboard",
        "TargMainBanner",
        "TargMainDraggerUnifiedTopBottom",
        "TargMainDraggerUnifiedSpectSurf",
        "TargMainDraggerUnifiedWhole",
        "TargMainDraggerUnifiedEnvDfmImp",
        "TargMainDraggerUnifiedDfmImp",
        "TargMainDraggerCollapsedWhole",
        "TargMainDraggerCollapsedMiddle",
        "TargMainDraggerCollapsedEnvSpect",
        "TargMainDraggerCollapsedSpectSurf",
        "TargEffectParam0",
        "TargEffectParam1",
        "TargEffectParam2",
        "TargEffectParam3",
        "TargEffectParam4",
        "TargEffectParam5",
        "TargEffectParam6",
        "TargEffectParam7",
        "TargEffectParam8",
        "TargEffectParam9",
        "TargEffectEnable",
    };

    String getString(const var& object, const Identifier& name, const String& fallback = {}) {
        return PresetJson::stringProperty(object, name, fallback);
    }

    bool getBool(const var& object, const Identifier& name, bool fallback = false) {
        return PresetJson::boolProperty(object, name, fallback);
    }

    double getDouble(const var& object, const Identifier& name, double fallback = 0.0) {
        return PresetJson::doubleProperty(object, name, fallback);
    }

    int getInt(const var& object, const Identifier& name, int fallback = 0) {
        return PresetJson::intProperty(object, name, fallback);
    }

    NotificationType getNotificationType(const var& object) {
        String notification = getString(object, "notification", "sync");

        if (notification == "none") {
            return dontSendNotification;
        }

        if (notification == "async") {
            return sendNotificationAsync;
        }

        return sendNotificationSync;
    }

    StringArray varPathToStringArray(const var& value) {
        StringArray path;

        if (const Array<var>* values = PresetJson::getArray(value)) {
            for (const auto& entry : *values) {
                path.add(entry.toString());
            }
        } else if (!value.isVoid()) {
            String pathString = value.toString();

            if (pathString.isNotEmpty()) {
                path.addTokens(pathString, "/", {});
            }
        }

        return path;
    }

    var pathToVar(const StringArray& path) {
        Array<var> values;

        for (const auto& part : path) {
            values.add(part);
        }

        return var(values);
    }

    int findMenuIndex(SynthMenuBarModel& model, const String& menuName) {
        StringArray names = model.getMenuBarNames();

        for (int i = 0; i < names.size(); ++i) {
            if (names[i] == menuName || names[i].equalsIgnoreCase(menuName)) {
                return i;
            }
        }

        return -1;
    }

    String menuNameForIndex(SynthMenuBarModel& model, int menuIndex) {
        StringArray names = model.getMenuBarNames();
        return isPositiveAndBelow(menuIndex, names.size()) ? names[menuIndex] : String();
    }

    int commandMenuIndex(SynthMenuBarModel& model, const var& command) {
        var index = PresetJson::property(command, "menuIndex");

        if (index.isVoid()) {
            index = PresetJson::property(command, "topLevelMenuIndex");
        }

        if (!index.isVoid()) {
            return int(index);
        }

        return findMenuIndex(model, getString(command, "menu"));
    }

    StringArray commandMenuPath(const var& command) {
        StringArray path = varPathToStringArray(PresetJson::property(command, "path"));

        if (path.isEmpty()) {
            String item = getString(command, "item", getString(command, "text"));

            if (item.isNotEmpty()) {
                path.add(item);
            }
        }

        return path;
    }

    void appendMenuItems(Array<var>& items,
                         const PopupMenu& menu,
                         const String& menuName,
                         int menuIndex,
                         const StringArray& parentPath) {
        PopupMenu::MenuItemIterator iterator(menu, false);

        while (iterator.next()) {
            const PopupMenu::Item& item = iterator.getItem();

            if (item.isSeparator || item.isSectionHeader) {
                continue;
            }

            StringArray itemPath(parentPath);
            itemPath.add(item.text);

            auto json = PresetJson::object();
            json->setProperty("menu", menuName);
            json->setProperty("menuIndex", menuIndex);
            json->setProperty("text", item.text);
            json->setProperty("itemId", item.itemID);
            json->setProperty("enabled", item.isEnabled);
            json->setProperty("ticked", item.isTicked);
            json->setProperty("triggerable", item.itemID != 0);
            json->setProperty("hasSubMenu", item.subMenu != nullptr);
            json->setProperty("path", pathToVar(itemPath));
            items.add(PresetJson::toVar(json));

            if (item.subMenu != nullptr) {
                appendMenuItems(items, *item.subMenu, menuName, menuIndex, itemPath);
            }
        }
    }

    bool findMenuItemByPath(const PopupMenu& menu,
                            const StringArray& path,
                            int depth,
                            PopupMenu::Item& found) {
        if (!isPositiveAndBelow(depth, path.size())) {
            return false;
        }

        PopupMenu::MenuItemIterator iterator(menu, false);

        while (iterator.next()) {
            const PopupMenu::Item& item = iterator.getItem();

            if (item.isSeparator || item.isSectionHeader || item.text != path[depth]) {
                continue;
            }

            if (depth == path.size() - 1) {
                found = item;
                return true;
            }

            if (item.subMenu != nullptr && findMenuItemByPath(*item.subMenu, path, depth + 1, found)) {
                return true;
            }
        }

        return false;
    }

    bool findMenuItemById(const PopupMenu& menu, int itemId, PopupMenu::Item& found) {
        PopupMenu::MenuItemIterator iterator(menu, false);

        while (iterator.next()) {
            const PopupMenu::Item& item = iterator.getItem();

            if (!item.isSeparator && !item.isSectionHeader && item.itemID == itemId) {
                found = item;
                return true;
            }

            if (item.subMenu != nullptr && findMenuItemById(*item.subMenu, itemId, found)) {
                return true;
            }
        }

        return false;
    }

    String modMatrixMenuKind(const var& command) {
        String kind = getString(command, "menu", getString(command, "kind", getString(command, "popup")));

        if (kind == "source" || kind == "input" || kind == "addInput") {
            return "input";
        }

        if (kind == "destination" || kind == "dest" || kind == "output" || kind == "addDestination") {
            return "output";
        }

        return {};
    }

    PopupMenu modMatrixMenuForKind(ModMatrixPanel& panel, const String& kind) {
        if (kind == "input") {
            return panel.getInputMenu();
        }

        if (kind == "output") {
            return panel.getOutputMenu(ModMatrixPanel::MeshTypes);
        }

        return {};
    }

    int modMatrixDimensionForCommand(const var& command) {
        var itemIdVar = PresetJson::property(command, "itemId");

        if (itemIdVar.isVoid()) {
            itemIdVar = PresetJson::property(command, "id");
        }

        if (!itemIdVar.isVoid()) {
            return int(itemIdVar) - 2;
        }

        String dim = getString(command, "dimension", getString(command, "dim", getString(command, "text"))).toLowerCase();

        if (dim == "none" || dim == "null" || dim == "off") {
            return ModMatrixPanel::NullDim;
        }

        if (dim == "red") {
            return ModMatrixPanel::RedDim;
        }

        if (dim == "blue") {
            return ModMatrixPanel::BlueDim;
        }

        return ModMatrixPanel::YellowDim;
    }

    void appendModMatrixDimensionItems(Array<var>& items,
                                       int inputId,
                                       int outputId,
                                       int oldDim,
                                       bool active) {
        struct DimensionItem {
            int itemId;
            int dim;
            const char* text;
        };

        static const DimensionItem dimensionItems[] = {
            { 1, ModMatrixPanel::NullDim, "None" },
            { 3, ModMatrixPanel::RedDim,  "Red"  },
            { 4, ModMatrixPanel::BlueDim, "Blue" },
        };

        for (const auto& dimensionItem : dimensionItems) {
            auto json = PresetJson::object();
            json->setProperty("menu", "ModMatrix:dimension");
            json->setProperty("inputId", inputId);
            json->setProperty("outputId", outputId);
            json->setProperty("text", dimensionItem.text);
            json->setProperty("itemId", dimensionItem.itemId);
            json->setProperty("dimension", dimensionItem.dim);
            json->setProperty("enabled", active);
            json->setProperty("ticked", oldDim == dimensionItem.dim);
            json->setProperty("triggerable", true);
            json->setProperty("path", pathToVar(StringArray(dimensionItem.text)));
            items.add(PresetJson::toVar(json));
        }
    }

    int envelopeGroupForCommand(const var& command, int currentGroup) {
        var groupId = PresetJson::property(command, "groupId");

        if (!groupId.isVoid()) {
            return int(groupId);
        }

        String group = getString(command, "group").toLowerCase();

        if (group == "volume" || group == "vol") {
            return LayerGroups::GroupVolume;
        }

        if (group == "pitch") {
            return LayerGroups::GroupPitch;
        }

        if (group == "scratch") {
            return LayerGroups::GroupScratch;
        }

        if (group == "wavepitch" || group == "wave-pitch" || group == "wave_pitch" || group == "oscphase") {
            return LayerGroups::GroupWavePitch;
        }

        return currentGroup;
    }

    var envelopeConfigPropsState(int groupId, const MeshLibrary::EnvProps& props) {
        auto groupName = [](int group) {
            switch (group) {
                case LayerGroups::GroupVolume:       return String("volume");
                case LayerGroups::GroupPitch:        return String("pitch");
                case LayerGroups::GroupScratch:      return String("scratch");
                case LayerGroups::GroupWavePitch:    return String("wavePitch");
                default:                             return "group" + String(group);
            }
        };

        auto json = PresetJson::object();
        json->setProperty("groupId", groupId);
        json->setProperty("group", groupName(groupId));
        json->setProperty("active", props.active);
        json->setProperty("dynamic", props.dynamic);
        json->setProperty("global", props.global);
        json->setProperty("logarithmic", props.logarithmic);
        json->setProperty("tempoSync", props.tempoSync);
        json->setProperty("scale", props.scale);
        json->setProperty("operating", props.isOperating());
        return PresetJson::toVar(json);
    }

    PopupMenu envelopeConfigMenuForProps(int groupId, const MeshLibrary::EnvProps& props) {
        PopupMenu menu;

        if (groupId != LayerGroups::GroupVolume) {
            menu.addItem(EnvelopeInter2D::CfgDynamic, "Dynamic while live", true, props.dynamic);
        } else {
            menu.addItem(EnvelopeInter2D::CfgLogarithmic, "Logarithmic", true, props.logarithmic);
        }

        if (groupId == LayerGroups::GroupScratch) {
            menu.addItem(EnvelopeInter2D::CfgGlobal, "Globally triggered", true, props.global);
        }

        menu.addItem(EnvelopeInter2D::CfgSyncTempo, "Sync to host tempo", true, props.tempoSync);

        PopupMenu scaleMenu;
        scaleMenu.addItem(EnvelopeInter2D::CfgScale1_16x, "1/16x", true, props.scale == -16);
        scaleMenu.addItem(EnvelopeInter2D::CfgScale1_4x,  "1/4x",  true, props.scale == -4);
        scaleMenu.addItem(EnvelopeInter2D::CfgScale1_2x,  "1/2x",  true, props.scale == -2);
        scaleMenu.addItem(EnvelopeInter2D::CfgScale1x,    "1x",    true, props.scale == 1);
        scaleMenu.addItem(EnvelopeInter2D::CfgScale2x,    "2x",    true, props.scale == 2);
        scaleMenu.addItem(EnvelopeInter2D::CfgScale4x,    "4x",    true, props.scale == 4);
        scaleMenu.addItem(EnvelopeInter2D::CfgScale16x,   "16x",   true, props.scale == 16);

        menu.addSubMenu("Duration scale", scaleMenu, true);
        return menu;
    }

    PopupMenu selectorMenuForPanel(SelectorPanel& selector) {
        PopupMenu menu;
        int size = selector.getSize();
        int currentIndex = selector.getCurrentIndexExternal();

        for (int i = 0; i < size; ++i) {
            menu.addItem(i + 1, String(i + 1), true, i == currentIndex);
        }

        return menu;
    }

    var selectorState(SelectorPanel& selector) {
        auto json = PresetJson::object();
        json->setProperty("currentIndex", selector.getCurrentIndexExternal());
        json->setProperty("displayIndex", selector.getCurrentIndexExternal() + 1);
        json->setProperty("itemCount", selector.getSize());
        return PresetJson::toVar(json);
    }

    var hoverSelectorState(HoverSelector& selector) {
        auto json = PresetJson::object();
        json->setProperty("menuActive", selector.menuActive);
        json->setProperty("horizontal", selector.horizontal);
        return PresetJson::toVar(json);
    }

    var makeResult(const String& type, bool ok, const String& message, const var& data = {}) {
        auto result = PresetJson::object();
        result->setProperty("type", type);
        result->setProperty("ok", ok);
        result->setProperty("message", message);

        if (!data.isVoid()) {
            result->setProperty("data", data);
        }

        return PresetJson::toVar(result);
    }

    var rectangleState(const Rectangle<int>& bounds) {
        auto json = PresetJson::object();
        json->setProperty("x", bounds.getX());
        json->setProperty("y", bounds.getY());
        json->setProperty("width", bounds.getWidth());
        json->setProperty("height", bounds.getHeight());
        return PresetJson::toVar(json);
    }

    var openGLDiagnosticsState(OpenGLBase& openGL, bool pollNow, const String& phase) {
        OpenGLBase::Diagnostics diagnostics = openGL.getDiagnostics(pollNow, phase);
        auto json = PresetJson::object();
        json->setProperty("attached", diagnostics.attached);
        json->setProperty("pollSucceeded", diagnostics.pollSucceeded);
        json->setProperty("width", diagnostics.width);
        json->setProperty("height", diagnostics.height);
        json->setProperty("renderingScale", diagnostics.renderingScale);
        json->setProperty("renderCount", diagnostics.renderCount);
        json->setProperty("contextCreateCount", diagnostics.contextCreateCount);
        json->setProperty("contextCloseCount", diagnostics.contextCloseCount);
        json->setProperty("errorCount", diagnostics.errorCount);
        json->setProperty("lastErrorCode", diagnostics.lastErrorCode);
        json->setProperty("lastErrorName", diagnostics.lastErrorName);
        json->setProperty("lastErrorPhase", diagnostics.lastErrorPhase);
        json->setProperty("polledErrorCode", diagnostics.polledErrorCode);
        json->setProperty("polledErrorName", diagnostics.polledErrorName);
        return PresetJson::toVar(json);
    }

    bool getPathSegmentValue(const var& source, const String& segment, var& result) {
        String propertyName = segment;
        int bracketIndex = propertyName.indexOfChar('[');
        int arrayIndex = -1;

        if (bracketIndex >= 0 && propertyName.endsWithChar(']')) {
            String indexString = propertyName.substring(bracketIndex + 1, propertyName.length() - 1);
            propertyName = propertyName.substring(0, bracketIndex);
            arrayIndex = indexString.getIntValue();
        }

        result = propertyName.isEmpty() ? source : PresetJson::property(source, propertyName);

        if (result.isVoid()) {
            return false;
        }

        if (arrayIndex >= 0) {
            if (auto* values = result.getArray()) {
                if (isPositiveAndBelow(arrayIndex, values->size())) {
                    result = values->getReference(arrayIndex);
                    return true;
                }
            }

            return false;
        }

        return true;
    }

    bool getPathValue(const var& source, const String& path, var& result) {
        if (path.isEmpty()) {
            result = source;
            return true;
        }

        StringArray segments;
        segments.addTokens(path, ".", {});
        result = source;

        for (const auto& segment : segments) {
            if (!getPathSegmentValue(result, segment, result)) {
                return false;
            }
        }

        return true;
    }

    bool numbersEqual(double lhs, double rhs, double tolerance) {
        return std::abs(lhs - rhs) <= tolerance;
    }

    bool compareVars(const var& actual, const String& op, const var& expected, double tolerance) {
        if (op == "exists") {
            return !actual.isVoid();
        }

        if (op == "notEquals") {
            return !compareVars(actual, "equals", expected, tolerance);
        }

        if (op == "equals") {
            if (actual.isDouble() || actual.isInt() || actual.isInt64() ||
                expected.isDouble() || expected.isInt() || expected.isInt64()) {
                return numbersEqual(double(actual), double(expected), tolerance);
            }

            return actual.toString() == expected.toString();
        }

        double actualNumber = double(actual);
        double expectedNumber = double(expected);

        if (op == "lessThan") {
            return actualNumber < expectedNumber;
        }

        if (op == "lessThanOrEqual") {
            return actualNumber <= expectedNumber || numbersEqual(actualNumber, expectedNumber, tolerance);
        }

        if (op == "greaterThan") {
            return actualNumber > expectedNumber;
        }

        if (op == "greaterThanOrEqual") {
            return actualNumber >= expectedNumber || numbersEqual(actualNumber, expectedNumber, tolerance);
        }

        return false;
    }

    String varTypeName(const var& value) {
        if (value.isBool()) {
            return "boolean";
        }

        if (value.isDouble() || value.isInt() || value.isInt64()) {
            return "number";
        }

        if (value.isString()) {
            return "string";
        }

        if (value.isArray()) {
            return "array";
        }

        if (value.isObject()) {
            return "object";
        }

        if (value.isVoid()) {
            return "void";
        }

        return "unknown";
    }

    var assertionOperatorsForValue(const var& value) {
        Array<var> operators;
        operators.add("equals");
        operators.add("notEquals");
        operators.add("exists");

        if (value.isDouble() || value.isInt() || value.isInt64()) {
            operators.add("lessThan");
            operators.add("lessThanOrEqual");
            operators.add("greaterThan");
            operators.add("greaterThanOrEqual");
        }

        return var(operators);
    }

    void appendAssertionPath(Array<var>& paths, const String& path, const var& value) {
        if (path.isEmpty()) {
            return;
        }

        auto json = PresetJson::object();
        json->setProperty("path", path);
        json->setProperty("type", varTypeName(value));
        json->setProperty("value", value);
        json->setProperty("assertions", assertionOperatorsForValue(value));
        paths.add(PresetJson::toVar(json));
    }

    void flattenAssertionPaths(const var& value, const String& path, Array<var>& paths, int depth, int maxDepth) {
        if (depth > maxDepth) {
            return;
        }

        if (auto* object = PresetJson::getObject(value)) {
            appendAssertionPath(paths, path, value);
            const NamedValueSet& properties = object->getProperties();

            for (int i = 0; i < properties.size(); ++i) {
                String childPath = path.isEmpty()
                    ? properties.getName(i).toString()
                    : path + "." + properties.getName(i).toString();

                flattenAssertionPaths(properties.getValueAt(i), childPath, paths, depth + 1, maxDepth);
            }

            return;
        }

        if (auto* array = value.getArray()) {
            appendAssertionPath(paths, path, value);

            for (int i = 0; i < array->size(); ++i) {
                flattenAssertionPaths(array->getReference(i), path + "[" + String(i) + "]", paths, depth + 1, maxDepth);
            }

            return;
        }

        appendAssertionPath(paths, path, value);
    }

    String assertionOperator(const var& command) {
        static const char* const operators[] = {
            "equals",
            "notEquals",
            "lessThan",
            "lessThanOrEqual",
            "greaterThan",
            "greaterThanOrEqual",
            "exists",
        };

        for (auto* op : operators) {
            if (!PresetJson::property(command, op).isVoid()) {
                return op;
            }
        }

        return {};
    }

    int getIdleDelayMs(const var& command) {
        double timeoutMs = getDouble(command, "timeoutMs", 50.0);
        return jlimit(0, 10000, int(getDouble(command, "idleDelayMs", getDouble(command, "delayMs", timeoutMs))));
    }

    bool drainMessageLoop(int delayMs) {
#if JUCE_MODAL_LOOPS_PERMITTED
        if (MessageManager::getInstance()->isThisTheMessageThread()) {
            return MessageManager::getInstance()->runDispatchLoopUntil(delayMs);
        }
#endif

        Thread::sleep(delayMs);
        return false;
    }

    bool drainMessageLoopIfRequested(const var& command) {
        if (!getBool(command, "waitForIdle")) {
            return false;
        }

        drainMessageLoop(getIdleDelayMs(command));
        return true;
    }

    Point<float> getPointerPosition(const var& command, Component& component, const String& xName, const String& yName) {
        Rectangle<int> bounds = component.getLocalBounds();
        var xValue = PresetJson::property(command, xName);
        var yValue = PresetJson::property(command, yName);

        if (!xValue.isVoid() && !yValue.isVoid()) {
            return { float(double(xValue)), float(double(yValue)) };
        }

        String normalizedXName = xName == "downX" ? "normalizedDownX" : "normalizedX";
        String normalizedYName = yName == "downY" ? "normalizedDownY" : "normalizedY";
        var normalizedX = PresetJson::property(command, normalizedXName);
        var normalizedY = PresetJson::property(command, normalizedYName);

        if ((normalizedX.isVoid() || normalizedY.isVoid()) && xName != "x" && yName != "y") {
            normalizedX = PresetJson::property(command, "normalizedX");
            normalizedY = PresetJson::property(command, "normalizedY");
        }

        if (!normalizedX.isVoid() && !normalizedY.isVoid()) {
            return {
                bounds.getWidth() * float(double(normalizedX)),
                bounds.getHeight() * float(double(normalizedY)),
            };
        }

        return bounds.getCentre().toFloat();
    }

    ModifierKeys getPointerModifiers(const var& command, bool buttonDown) {
        ModifierKeys modifiers = ModifierKeys::currentModifiers.withoutMouseButtons();

        if (buttonDown) {
            String button = getString(command, "button", getString(command, "mouseButton")).toLowerCase();
            bool rightButton = getBool(command, "right") || button == "right" || button == "secondary";
            bool middleButton = getBool(command, "middle") || button == "middle";

            if (rightButton) {
                modifiers = modifiers.withFlags(ModifierKeys::rightButtonModifier);
            } else if (middleButton) {
                modifiers = modifiers.withFlags(ModifierKeys::middleButtonModifier);
            } else {
                modifiers = modifiers.withFlags(ModifierKeys::leftButtonModifier);
            }
        }

        if (getBool(command, "shift")) {
            modifiers = modifiers.withFlags(ModifierKeys::shiftModifier);
        }

        if (getBool(command, "command")) {
            modifiers = modifiers.withFlags(ModifierKeys::commandModifier);
        }

        if (getBool(command, "ctrl")) {
            modifiers = modifiers.withFlags(ModifierKeys::ctrlModifier);
        }

        if (getBool(command, "alt")) {
            modifiers = modifiers.withFlags(ModifierKeys::altModifier);
        }

        return modifiers;
    }

    MouseEvent makePointerEvent(Component& component,
                                Point<float> position,
                                Point<float> downPosition,
                                const var& command,
                                bool buttonDown,
                                bool wasDragged,
                                int clickCount) {
        Time now = Time::getCurrentTime();

        return {
            Desktop::getInstance().getMainMouseSource(),
            position,
            getPointerModifiers(command, buttonDown),
            MouseInputSource::defaultPressure,
            0.0f, 0.0f, 0.0f, 0.0f,
            &component,
            &component,
            now,
            downPosition,
            now,
            clickCount,
            wasDragged,
        };
    }

    String componentRole(Component* component) {
        if (dynamic_cast<Slider*>(component) != nullptr) {
            return "slider";
        }

        if (dynamic_cast<Button*>(component) != nullptr) {
            return "button";
        }

        if (dynamic_cast<ComboBox*>(component) != nullptr) {
            return "comboBox";
        }

        if (dynamic_cast<TextEditor*>(component) != nullptr) {
            return "textEditor";
        }

        if (dynamic_cast<Label*>(component) != nullptr) {
            return "label";
        }

        if (dynamic_cast<Viewport*>(component) != nullptr) {
            return "viewport";
        }

        return "component";
    }

    String resolveSavableSourceName(const String& source) {
        static const std::pair<const char*, const char*> aliases[] = {
            { "meshLibrary",    "MeshLibrary" },
            { "morphPanel",     "MorphPanel" },
            { "mainPanel",      "MainPanel" },
            { "effects",        "EffectGuiRegistry" },
            { "modMatrix",      "ModMatrixPanel" },
            { "envelopes",      "Envelope2D" },
            { "envelopeProps",  "Envelope2D" },
            { "guideCurves",    "GuideCurvePanel" },
            { "oscControls",    "OscControlPanel" },
            { "settings",       "Settings" },
        };

        for (const auto& alias : aliases) {
            if (source == alias.first) {
                return alias.second;
            }
        }

        return source;
    }

    void setIfPathExists(DynamicObject& object, const Identifier& name, const var& source, const String& path) {
        var value;

        if (getPathValue(source, path, value)) {
            object.setProperty(name, value);
        }
    }

    String meshLayerGroupName(int group) {
        switch (group) {
            case LayerGroups::GroupVolume:       return "volume";
            case LayerGroups::GroupPitch:        return "pitch";
            case LayerGroups::GroupScratch:      return "scratch";
            case LayerGroups::GroupGuideCurve:   return "guideCurve";
            case LayerGroups::GroupTime:         return "time";
            case LayerGroups::GroupSpect:        return "spect";
            case LayerGroups::GroupPhase:        return "phase";
            case LayerGroups::GroupOscPhase:     return "oscPhase";
            case LayerGroups::GroupWavePitch:    return "wavePitch";
            case LayerGroups::GroupWaveshaper:   return "waveshaper";
            case LayerGroups::GroupIrModeller:   return "irModeller";
            default:                             return "group" + String(group);
        }
    }

    String meshTypeName(int type) {
        switch (type) {
            case MeshLibrary::TypeMesh:      return "mesh";
            case MeshLibrary::TypeMesh2D:    return "mesh2D";
            case MeshLibrary::TypeEnvelope:  return "envelope";
            default:                         return "unknown";
        }
    }

    bool meshGroupMatches(const var& command, int group, const String& name) {
        var requested = PresetJson::property(command, "group");

        if (requested.isVoid()) {
            requested = PresetJson::property(command, "groupId");
        }

        if (requested.isVoid()) {
            return true;
        }

        if (requested.isString()) {
            return requested.toString() == name;
        }

        return int(requested) == group;
    }

    int meshGroupIdForCommand(const var& command, MeshLibrary& meshLibrary) {
        var requested = PresetJson::property(command, "group");

        if (requested.isVoid()) {
            requested = PresetJson::property(command, "groupId");
        }

        if (requested.isVoid()) {
            return CommonEnums::Null;
        }

        if (!requested.isString()) {
            int groupId = int(requested);
            return isPositiveAndBelow(groupId, meshLibrary.getNumGroups()) ? groupId : CommonEnums::Null;
        }

        String groupName = requested.toString();

        for (int groupId = 0; groupId < meshLibrary.getNumGroups(); ++groupId) {
            if (meshLayerGroupName(groupId) == groupName) {
                return groupId;
            }
        }

        return CommonEnums::Null;
    }

    int meshLayerIndexForCommand(const var& command, const MeshLibrary::LayerGroup& group) {
        var requested = PresetJson::property(command, "layer");

        if (requested.isVoid()) {
            requested = PresetJson::property(command, "layerIndex");
        }

        if (requested.isVoid() || requested.toString() == "current") {
            return group.current;
        }

        int layerIndex = int(requested);
        return isPositiveAndBelow(layerIndex, group.size()) ? layerIndex : CommonEnums::Null;
    }

    int meshGroupIdForMutationCommand(const var& command, MeshLibrary& meshLibrary, const String& area) {
        int groupId = meshGroupIdForCommand(command, meshLibrary);

        if (groupId != CommonEnums::Null) {
            return groupId;
        }

        if (area == "AreaWfrmWaveform3D") {
            return isPositiveAndBelow(LayerGroups::GroupTime, meshLibrary.getNumGroups())
                ? LayerGroups::GroupTime
                : CommonEnums::Null;
        }

        return CommonEnums::Null;
    }

    String meshAreaForCommand(const var& command) {
        String area = getString(command, "area");

        if (area.isNotEmpty()) {
            return area;
        }

        String group = getString(command, "group");

        if (group == "time" || group.isEmpty()) {
            return "AreaWfrmWaveform3D";
        }

        if (group == "waveshaper") {
            return "AreaWshpEditor";
        }

        if (group == "guideCurve") {
            return "AreaGuideCurves";
        }

        return {};
    }

    bool runMeshTourAction(CycleTour& tour,
                           const String& actionType,
                           const String& area,
                           int data1,
                           float pointX,
                           float pointY,
                           float value,
                           bool updates) {
        XmlElement actionElem("Action");
        actionElem.setAttribute("type", actionType);
        actionElem.setAttribute("area", area);
        actionElem.setAttribute("data1", data1);
        actionElem.setAttribute("point-x", pointX);
        actionElem.setAttribute("point-y", pointY);
        actionElem.setAttribute("value", value);
        actionElem.setAttribute("updates", updates);

        CycleTour::Action action;
        tour.readAction(action, &actionElem);
        tour.performAction(action);
        return action.hasExecuted || actionType == "SelectPoint" || actionType == "DeselectPoints";
    }

    bool moveMeshVertexDirectly(MeshLibrary& meshLibrary,
                                const var& command,
                                const String& area,
                                int vertexIndex,
                                float dx,
                                float dy) {
        ScopedLock sl(meshLibrary.getLock());
        int groupId = meshGroupIdForMutationCommand(command, meshLibrary, area);

        if (groupId == CommonEnums::Null) {
            return false;
        }

        MeshLibrary::LayerGroup& group = meshLibrary.getLayerGroup(groupId);
        int layerIndex = meshLayerIndexForCommand(command, group);

        if (layerIndex == CommonEnums::Null) {
            return false;
        }

        Mesh* mesh = group.layers[layerIndex].mesh;

        if (mesh == nullptr || !isPositiveAndBelow(vertexIndex, mesh->getNumVerts())) {
            return false;
        }

        Vertex* vertex = mesh->getVerts()[vertexIndex];
        vertex->values[Vertex::Phase] += dx;
        vertex->values[Vertex::Amp] += dy;
        mesh->validate();
        return true;
    }

    bool deleteMeshVertexDirectly(MeshLibrary& meshLibrary,
                                  const var& command,
                                  const String& area,
                                  int vertexIndex) {
        ScopedLock sl(meshLibrary.getLock());
        int groupId = meshGroupIdForMutationCommand(command, meshLibrary, area);

        if (groupId == CommonEnums::Null) {
            return false;
        }

        MeshLibrary::LayerGroup& group = meshLibrary.getLayerGroup(groupId);
        int layerIndex = meshLayerIndexForCommand(command, group);

        if (layerIndex == CommonEnums::Null) {
            return false;
        }

        Mesh* mesh = group.layers[layerIndex].mesh;

        if (mesh == nullptr || !isPositiveAndBelow(vertexIndex, mesh->getNumVerts())) {
            return false;
        }

        Vertex* vertex = mesh->getVerts()[vertexIndex];
        Array<VertCube*> cubesToDelete = vertex->owners;

        for (auto* cube : cubesToDelete) {
            if (cube == nullptr || !mesh->removeCube(cube)) {
                continue;
            }

            for (int i = 0; i < VertCube::numVerts; ++i) {
                Vertex* cubeVertex = cube->getVertex(i);

                if (cubeVertex == nullptr) {
                    continue;
                }

                if (cubeVertex->getNumOwners() == 1) {
                    mesh->removeVert(cubeVertex);
                } else {
                    cubeVertex->removeOwner(cube);
                }
            }
        }

        group.selected.clear();
        mesh->validate();
        return true;
    }

    int tourAreaForMeshArea(const String& area) {
        if (area == "AreaWfrmWaveform3D") {
            return CycleTour::AreaWfrmWaveform3D;
        }

        if (area == "AreaWshpEditor") {
            return CycleTour::AreaWshpEditor;
        }

        if (area == "AreaGuideCurves") {
            return CycleTour::AreaGuideCurves;
        }

        if (area == "AreaSpectrogram") {
            return CycleTour::AreaSpectrogram;
        }

        if (area == "AreaSpectrum") {
            return CycleTour::AreaSpectrum;
        }

        if (area == "AreaEnvelopes") {
            return CycleTour::AreaEnvelopes;
        }

        return CycleTour::AreaNull;
    }

    ModifierKeys meshGestureModifiers(bool buttonDown, bool rightButton, bool shiftDown, bool commandDown) {
        ModifierKeys modifiers = ModifierKeys::currentModifiers.withoutMouseButtons();

        if (!buttonDown) {
            if (shiftDown) {
                modifiers = modifiers.withFlags(ModifierKeys::shiftModifier);
            }

            if (commandDown) {
                modifiers = modifiers.withFlags(ModifierKeys::commandModifier);
            }

            return modifiers;
        }

        modifiers = modifiers.withFlags(rightButton ? ModifierKeys::rightButtonModifier : ModifierKeys::leftButtonModifier);

        if (shiftDown) {
            modifiers = modifiers.withFlags(ModifierKeys::shiftModifier);
        }

        if (commandDown) {
            modifiers = modifiers.withFlags(ModifierKeys::commandModifier);
        }

        return modifiers;
    }

    MouseEvent makeMeshGestureEvent(Component& component,
                                    Point<float> position,
                                    Point<float> downPosition,
                                    bool buttonDown,
                                    bool rightButton,
                                    bool wasDragged,
                                    bool shiftDown = false,
                                    bool commandDown = false) {
        Time now = Time::getCurrentTime();

        return {
            Desktop::getInstance().getMainMouseSource(),
            position,
            meshGestureModifiers(buttonDown, rightButton, shiftDown, commandDown),
            MouseInputSource::defaultPressure,
            0.0f, 0.0f, 0.0f, 0.0f,
            &component,
            &component,
            now,
            downPosition,
            now,
            1,
            wasDragged,
        };
    }

    Point<float> meshPointToLocal(Interactor& interactor, float x, float y) {
        Panel* panel = interactor.panel.get();

        if (panel == nullptr || panel->getComponent() == nullptr) {
            return {};
        }

        Rectangle<int> bounds = panel->getComponent()->getLocalBounds();
        float localX = jlimit(0.0f, float(jmax(0, bounds.getWidth() - 1)), panel->sx(x));
        float localY = jlimit(0.0f, float(jmax(0, bounds.getHeight() - 1)), panel->sy(y));

        return { localX, localY };
    }

    bool meshVertexPointToLocal(Interactor& interactor, int vertexIndex, Point<float>& position) {
        Mesh* mesh = interactor.getMesh();

        if (mesh == nullptr || !isPositiveAndBelow(vertexIndex, mesh->getNumVerts())) {
            return false;
        }

        Vertex* vertex = mesh->getVerts()[vertexIndex];

        if (vertex == nullptr) {
            return false;
        }

        float x = vertex->values[interactor.dims.x];
        float y = vertex->values[interactor.dims.y];

        if (interactor.rasterizerWrapsVertices()) {
            if (interactor.dims.x == Vertex::Phase && x > 1.0f) {
                x -= 1.0f;
            }

            if (interactor.dims.y == Vertex::Phase && y > 1.0f) {
                y -= 1.0f;
            }
        }

        position = meshPointToLocal(interactor, x, y);
        return true;
    }

    void dispatchMeshClick(Interactor& interactor,
                           Point<float> position,
                           bool rightButton,
                           bool shiftDown = false,
                           bool commandDown = false) {
        Component& component = *interactor.display.get();
        MouseEvent enterEvent = makeMeshGestureEvent(component, position, position, false, rightButton, false, shiftDown, commandDown);
        MouseEvent moveEvent = makeMeshGestureEvent(component, position, position, false, rightButton, false, shiftDown, commandDown);
        MouseEvent downEvent = makeMeshGestureEvent(component, position, position, true, rightButton, false, shiftDown, commandDown);
        MouseEvent upEvent = makeMeshGestureEvent(component, position, position, false, rightButton, false, shiftDown, commandDown);

        interactor.mouseEnter(enterEvent);
        interactor.mouseMove(moveEvent);
        interactor.mouseDown(downEvent);
        interactor.mouseUp(upEvent);
    }

    void dispatchMeshDrag(Interactor& interactor,
                          Point<float> startPosition,
                          Point<float> endPosition,
                          bool rightButton,
                          bool shiftDown = false,
                          bool commandDown = false) {
        Component& component = *interactor.display.get();
        Point<float> midpoint = startPosition + (endPosition - startPosition) * 0.5f;
        MouseEvent enterEvent = makeMeshGestureEvent(component, startPosition, startPosition, false, rightButton, false, shiftDown, commandDown);
        MouseEvent moveEvent = makeMeshGestureEvent(component, startPosition, startPosition, false, rightButton, false, shiftDown, commandDown);
        MouseEvent downEvent = makeMeshGestureEvent(component, startPosition, startPosition, true, rightButton, false, shiftDown, commandDown);
        MouseEvent dragMidEvent = makeMeshGestureEvent(component, midpoint, startPosition, true, rightButton, true, shiftDown, commandDown);
        MouseEvent dragEndEvent = makeMeshGestureEvent(component, endPosition, startPosition, true, rightButton, true, shiftDown, commandDown);
        MouseEvent upEvent = makeMeshGestureEvent(component, endPosition, startPosition, false, rightButton, true, shiftDown, commandDown);

        interactor.mouseEnter(enterEvent);
        interactor.mouseMove(moveEvent);
        interactor.mouseDown(downEvent);
        interactor.mouseDrag(dragMidEvent);
        interactor.mouseDrag(dragEndEvent);
        interactor.mouseUp(upEvent);
    }

    bool runMeshGesture(CycleTour& tour,
                        KeyboardInputHandler& keyboard,
                        MeshLibrary& meshLibrary,
                        Settings& settings,
                        const var& command,
                        const String& type,
                        const String& area,
                        int vertexIndex,
                        float x,
                        float y,
                        float dx,
                        float dy,
                        String& message) {
        int areaId = tourAreaForMeshArea(area);
        Interactor* interactor = areaId != CycleTour::AreaNull ? tour.areaToInteractor(areaId) : nullptr;

        if (interactor == nullptr || interactor->panel == nullptr || interactor->display == nullptr) {
            message = "Mesh gesture target could not be resolved: " + area;
            return false;
        }

        if (!interactor->display->isShowing()) {
            message = "Mesh gesture target is not showing: " + area;
            return false;
        }

        int groupId = meshGroupIdForMutationCommand(command, meshLibrary, area);

        if (groupId == CommonEnums::Null || groupId != interactor->layerType) {
            message = "Mesh gesture group does not match target interactor";
            return false;
        }

        MeshLibrary::LayerGroup& group = meshLibrary.getLayerGroup(groupId);
        int layerIndex = meshLayerIndexForCommand(command, group);

        if (layerIndex == CommonEnums::Null || layerIndex != group.current) {
            message = "Mesh gesture requires the current visible layer";
            return false;
        }

        const bool selectWithRight = settings.getGlobalSetting(AppSettings::SelectWithRight) == 1;
        const bool selectButtonIsRight = selectWithRight;
        const bool createButtonIsRight = !selectWithRight;
        int previousTool = settings.getGlobalSetting(AppSettings::Tool);
        settings.getGlobalSetting(AppSettings::Tool) = Tools::Selector;
        keyboard.setFocusedInteractor(interactor, true);

        if (type == "addVertex") {
            dispatchMeshClick(*interactor, meshPointToLocal(*interactor, x, y), createButtonIsRight);
        } else if (type == "selectVertex") {
            Point<float> position;

            if (!meshVertexPointToLocal(*interactor, vertexIndex, position)) {
                settings.getGlobalSetting(AppSettings::Tool) = previousTool;
                message = "selectVertex could not resolve vertex";
                return false;
            }

            dispatchMeshClick(*interactor, position, selectButtonIsRight);
        } else if (type == "moveVertex") {
            Point<float> startPosition;

            if (!meshVertexPointToLocal(*interactor, vertexIndex, startPosition)) {
                settings.getGlobalSetting(AppSettings::Tool) = previousTool;
                message = "moveVertex could not resolve vertex";
                return false;
            }

            Vertex2 startMesh(interactor->panel->invertScaleX(int(startPosition.x)),
                              interactor->panel->invertScaleY(int(startPosition.y)));
            Point<float> endPosition = meshPointToLocal(*interactor, startMesh.x + dx, startMesh.y + dy);
            dispatchMeshDrag(*interactor, startPosition, endPosition, selectButtonIsRight);
        } else if (type == "deleteVertex") {
            Point<float> position;

            if (!meshVertexPointToLocal(*interactor, vertexIndex, position)) {
                settings.getGlobalSetting(AppSettings::Tool) = previousTool;
                message = "deleteVertex could not resolve vertex";
                return false;
            }

            dispatchMeshClick(*interactor, position, selectButtonIsRight);
            keyboard.keyPressed(KeyPress(KeyPress::deleteKey), interactor->display.get());
        } else {
            settings.getGlobalSetting(AppSettings::Tool) = previousTool;
            message = "Unknown mesh gesture command: " + type;
            return false;
        }

        settings.getGlobalSetting(AppSettings::Tool) = previousTool;
        return true;
    }

    Interactor* resolveMeshGestureInteractor(CycleTour& tour,
                                             MeshLibrary& meshLibrary,
                                             const var& command,
                                             const String& area,
                                             String& message) {
        int areaId = tourAreaForMeshArea(area);
        Interactor* interactor = areaId != CycleTour::AreaNull ? tour.areaToInteractor(areaId) : nullptr;

        if (interactor == nullptr || interactor->panel == nullptr || interactor->display == nullptr) {
            message = "Mesh gesture target could not be resolved: " + area;
            return nullptr;
        }

        if (!interactor->display->isShowing()) {
            message = "Mesh gesture target is not showing: " + area;
            return nullptr;
        }

        int groupId = meshGroupIdForMutationCommand(command, meshLibrary, area);

        if (groupId == CommonEnums::Null || groupId != interactor->layerType) {
            message = "Mesh gesture group does not match target interactor";
            return nullptr;
        }

        MeshLibrary::LayerGroup& group = meshLibrary.getLayerGroup(groupId);
        int layerIndex = meshLayerIndexForCommand(command, group);

        if (layerIndex == CommonEnums::Null || layerIndex != group.current) {
            message = "Mesh gesture requires the current visible layer";
            return nullptr;
        }

        return interactor;
    }

    int selectionHandleForName(const String& handle) {
        if (handle == "left") {
            return PanelState::Left;
        }

        if (handle == "top") {
            return PanelState::Top;
        }

        if (handle == "right") {
            return PanelState::Right;
        }

        if (handle == "bottom" || handle == "bot") {
            return PanelState::Bot;
        }

        if (handle == "move" || handle == "center" || handle == "centre") {
            return PanelState::MoveHandle;
        }

        return CommonEnums::Null;
    }

    Point<float> selectionHandlePoint(Interactor& interactor, int handle) {
        if (handle == PanelState::MoveHandle) {
            return interactor.finalSelection.getCentre().toFloat();
        }

        if (!isPositiveAndBelow(handle, int(interactor.selectionCorners.size()))) {
            return {};
        }

        Vertex2 start = interactor.selectionCorners[handle];
        Vertex2 end = interactor.selectionCorners[(handle + 1) % interactor.selectionCorners.size()];
        Vertex2 midpoint = (start + end) * 0.5f;

        return { midpoint.x, midpoint.y };
    }

    var meshSelectionState(Interactor& interactor) {
        auto json = PresetJson::object();
        json->setProperty("selectedCount", int(interactor.getSelected().size()));
        json->setProperty("selectedFrameCount", int(interactor.state.selectedFrame.size()));
        json->setProperty("highlitCorner", interactor.getStateValue(HighlitCorner));
        json->setProperty("finalSelection", rectangleState(interactor.finalSelection));
        return PresetJson::toVar(json);
    }

    var meshSummary(Mesh* mesh) {
        auto json = PresetJson::object();
        json->setProperty("resolved", mesh != nullptr);

        if (mesh != nullptr) {
            json->setProperty("name", mesh->getName());
            json->setProperty("version", mesh->getVersion());
            json->setProperty("vertices", mesh->getNumVerts());
            json->setProperty("cubes", mesh->getNumCubes());
            json->setProperty("hasEnoughCubesForCrossSection", mesh->hasEnoughCubesForCrossSection());
        }

        return PresetJson::toVar(json);
    }

    var layerSummary(int groupId, const String& groupName, int layerIndex, int currentIndex, const MeshLibrary::Layer& layer) {
        auto json = PresetJson::object();
        json->setProperty("groupId", groupId);
        json->setProperty("group", groupName);
        json->setProperty("index", layerIndex);
        json->setProperty("current", layerIndex == currentIndex);
        json->setProperty("address", groupName + "[" + String(layerIndex) + "]");
        json->setProperty("mesh", meshSummary(layer.mesh));

        if (layer.props != nullptr) {
            json->setProperty("properties", layer.props->writeJSON());
        } else {
            json->setProperty("properties", {});
        }

        return PresetJson::toVar(json);
    }

    var exportedMeshLayerState(int groupId,
                               const String& groupName,
                               int meshType,
                               int layerIndex,
                               int currentIndex,
                               const MeshLibrary::Layer& layer) {
        auto json = PresetJson::object();
        json->setProperty("groupId", groupId);
        json->setProperty("group", groupName);
        json->setProperty("meshType", meshType);
        json->setProperty("meshTypeName", meshTypeName(meshType));
        json->setProperty("layerIndex", layerIndex);
        json->setProperty("current", layerIndex == currentIndex);
        json->setProperty("address", groupName + "[" + String(layerIndex) + "]");
        json->setProperty("summary", layerSummary(groupId, groupName, layerIndex, currentIndex, layer));

        if (layer.mesh != nullptr) {
            json->setProperty("mesh", layer.mesh->writeJSON());
        } else {
            json->setProperty("mesh", {});
        }

        if (layer.props != nullptr) {
            json->setProperty("properties", layer.props->writeJSON());
        } else {
            json->setProperty("properties", {});
        }

        return PresetJson::toVar(json);
    }

    struct ScheduledMidiEvent {
        int sampleOffset{};
        MidiMessage message;
    };

    int midiEventSample(const var& event, double sampleRate, int totalSamples) {
        var sampleValue = PresetJson::property(event, "sample");

        if (!sampleValue.isVoid()) {
            return jlimit(0, totalSamples, int(sampleValue));
        }

        double timeMs = getDouble(event, "timeMs", getDouble(event, "time", 0.0));
        return jlimit(0, totalSamples, int(std::round(timeMs * sampleRate / 1000.0)));
    }

    bool addScheduledMidiEvent(Array<ScheduledMidiEvent>& events,
                               const MidiMessage& midiMessage,
                               int sampleOffset) {
        ScheduledMidiEvent event;
        event.sampleOffset = sampleOffset;
        event.message = midiMessage;
        events.add(event);
        return true;
    }

    bool addScheduledMidiEvent(Array<ScheduledMidiEvent>& events,
                               const var& event,
                               double sampleRate,
                               int totalSamples,
                               String& message) {
        String type = getString(event, "type", getString(event, "event", "noteOn"));
        int channel = jlimit(1, 16, int(getDouble(event, "channel", 1.0)));
        int sampleOffset = midiEventSample(event, sampleRate, totalSamples);

        if (type == "noteOn" || type == "note") {
            int note = jlimit(0, 127, int(getDouble(event, "note", 60.0)));
            float velocity = jlimit(0.0f, 1.0f, float(getDouble(event, "velocity", 0.8)));
            return addScheduledMidiEvent(events, MidiMessage::noteOn(channel, note, velocity), sampleOffset);
        }

        if (type == "noteOff") {
            int note = jlimit(0, 127, int(getDouble(event, "note", 60.0)));
            return addScheduledMidiEvent(events, MidiMessage::noteOff(channel, note), sampleOffset);
        }

        if (type == "controller") {
            int controller = jlimit(0, 127, int(getDouble(event, "controller", getDouble(event, "number", 0.0))));
            int value = jlimit(0, 127, int(getDouble(event, "value", 0.0)));
            return addScheduledMidiEvent(events, MidiMessage::controllerEvent(channel, controller, value), sampleOffset);
        }

        if (type == "pitchWheel") {
            int value = jlimit(0, 16383, int(getDouble(event, "value", 8192.0)));
            return addScheduledMidiEvent(events, MidiMessage::pitchWheel(channel, value), sampleOffset);
        }

        if (type == "allNotesOff") {
            return addScheduledMidiEvent(events, MidiMessage::allNotesOff(channel), sampleOffset);
        }

        message = "Unsupported MIDI event type: " + type;
        return false;
    }

    bool buildMidiSchedule(const var& command,
                           Array<ScheduledMidiEvent>& events,
                           double sampleRate,
                           int totalSamples,
                           String& message) {
        var commandEvents = PresetJson::property(command, "events");

        if (const Array<var>* eventArray = PresetJson::getArray(commandEvents)) {
            for (const auto& event : *eventArray) {
                if (!addScheduledMidiEvent(events, event, sampleRate, totalSamples, message)) {
                    return false;
                }
            }
        }

        var noteValue = PresetJson::property(command, "note");

        if (!noteValue.isVoid()) {
            int channel = jlimit(1, 16, int(getDouble(command, "channel", 1.0)));
            int note = jlimit(0, 127, int(noteValue));
            float velocity = jlimit(0.0f, 1.0f, float(getDouble(command, "velocity", 0.8)));
            int noteOffSample = midiEventSample(command, sampleRate, totalSamples);
            noteOffSample += int(std::round(getDouble(command, "noteDurationMs", 800.0) * sampleRate / 1000.0));
            noteOffSample = jlimit(0, totalSamples, noteOffSample);

            addScheduledMidiEvent(events, MidiMessage::noteOn(channel, note, velocity), 0);
            addScheduledMidiEvent(events, MidiMessage::noteOff(channel, note), noteOffSample);
        }

        std::sort(events.begin(), events.end(), [](const ScheduledMidiEvent& lhs, const ScheduledMidiEvent& rhs) {
            return lhs.sampleOffset < rhs.sampleOffset;
        });

        return true;
    }

    var audioCaptureMetrics(AudioSampleBuffer& capture, double sampleRate) {
        int channels = capture.getNumChannels();
        int totalSamples = capture.getNumSamples();
        double sumSquares = 0.0;
        float peak = 0.0f;
        Array<var> channelMetrics;

        for (int ch = 0; ch < channels; ++ch) {
            Buffer<float> samples(capture, ch);
            ScopedAlloc<float> magnitudes(totalSamples);
            samples.copyTo(magnitudes);
            magnitudes.abs();

            float channelPeak = totalSamples > 0 ? magnitudes.max() : 0.0f;
            double channelNorm = totalSamples > 0 ? double(samples.normL2()) : 0.0;
            double channelRms = totalSamples > 0 ? channelNorm / std::sqrt(double(totalSamples)) : 0.0;

            peak = jmax(peak, channelPeak);
            sumSquares += channelNorm * channelNorm;

            auto channelJson = PresetJson::object();
            channelJson->setProperty("channel", ch);
            channelJson->setProperty("peak", channelPeak);
            channelJson->setProperty("rms", channelRms);
            channelMetrics.add(PresetJson::toVar(channelJson));
        }

        double rmsDenominator = double(jmax(1, channels * totalSamples));
        double rms = std::sqrt(sumSquares / rmsDenominator);

        auto json = PresetJson::object();
        json->setProperty("sampleRate", sampleRate);
        json->setProperty("channels", channels);
        json->setProperty("samples", totalSamples);
        json->setProperty("durationMs", sampleRate > 0.0 ? 1000.0 * double(totalSamples) / sampleRate : 0.0);
        json->setProperty("peak", peak);
        json->setProperty("rms", rms);
        json->setProperty("channelMetrics", var(channelMetrics));
        return PresetJson::toVar(json);
    }

    bool checkAudioThreshold(const var& command,
                             const var& metrics,
                             const String& commandProperty,
                             const String& metricProperty,
                             const String& op,
                             String& message) {
        var expected = PresetJson::property(command, commandProperty);

        if (expected.isVoid()) {
            return true;
        }

        var actual = PresetJson::property(metrics, metricProperty);

        if (compareVars(actual, op, expected, 0.0)) {
            return true;
        }

        message = "Audio assertion failed: " + metricProperty + " " + actual.toString()
                + " was not " + op + " " + expected.toString();
        return false;
    }

    bool checkAudioThresholds(const var& command, const var& metrics, String& message) {
        return checkAudioThreshold(command, metrics, "peakGreaterThan", "peak", "greaterThan", message)
            && checkAudioThreshold(command, metrics, "peakLessThan", "peak", "lessThan", message)
            && checkAudioThreshold(command, metrics, "rmsGreaterThan", "rms", "greaterThan", message)
            && checkAudioThreshold(command, metrics, "rmsLessThan", "rms", "lessThan", message);
    }
}

CycleAutomation::CycleAutomation(SingletonRepo* repo) :
        SingletonAccessor(repo, "CycleAutomation") {
}

CycleAutomation::~CycleAutomation() {
    sessionServer = nullptr;
}

class CycleAutomation::SessionServer :
        public Thread {
public:
    SessionServer(CycleAutomation& owner, String socketPath) :
            Thread("CycleAutomationSession")
        ,   owner(owner)
        ,   socketPath(std::move(socketPath)) {
    }

    ~SessionServer() override {
        signalThreadShouldExit();
        closeServerSocket();
        stopThread(1000);
      #if JUCE_MAC || JUCE_LINUX
        ::unlink(socketPath.toRawUTF8());
      #else
        File(socketPath).deleteFile();
      #endif
    }

    bool start(String& message) {
      #if JUCE_MAC || JUCE_LINUX
        ::unlink(socketPath.toRawUTF8());

        serverFd = ::socket(AF_UNIX, SOCK_STREAM, 0);

        if (serverFd < 0) {
            message = "Could not create session socket: " + String(std::strerror(errno));
            return false;
        }

        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        String::CharPointerType pathChars = socketPath.getCharPointer();
        std::strncpy(address.sun_path, pathChars.getAddress(), sizeof(address.sun_path) - 1);

        if (::bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            message = "Could not bind session socket: " + String(std::strerror(errno));
            closeServerSocket();
            return false;
        }

        if (::listen(serverFd, 8) != 0) {
            message = "Could not listen on session socket: " + String(std::strerror(errno));
            closeServerSocket();
            return false;
        }

        startThread();
        message = "Cycle automation session listening: " + socketPath;
        return true;
      #else
        ignoreUnused(message);
        return false;
      #endif
    }

    void run() override {
      #if JUCE_MAC || JUCE_LINUX
        while (!threadShouldExit()) {
            int clientFd = ::accept(serverFd, nullptr, nullptr);

            if (clientFd < 0) {
                if (!threadShouldExit()) {
                    Thread::sleep(25);
                }

                continue;
            }

            handleClient(clientFd);
            ::close(clientFd);
        }
      #endif
    }

private:
    void closeServerSocket() {
      #if JUCE_MAC || JUCE_LINUX
        if (serverFd >= 0) {
            ::shutdown(serverFd, SHUT_RDWR);
            ::close(serverFd);
            serverFd = -1;
        }
      #endif
    }

    void handleClient(int clientFd) {
      #if JUCE_MAC || JUCE_LINUX
        String requestText;
        char buffer[1024];

        while (!threadShouldExit()) {
            ssize_t count = ::read(clientFd, buffer, sizeof(buffer));

            if (count <= 0) {
                break;
            }

            requestText += String::fromUTF8(buffer, int(count));

            if (requestText.containsChar('\n')) {
                requestText = requestText.upToFirstOccurrenceOf("\n", false, false);
                break;
            }
        }

        var request = JSON::parse(requestText);
        auto completed = std::make_shared<WaitableEvent>();
        auto response = std::make_shared<var>();

        bool dispatched = MessageManager::callAsync([this, request, response, completed] {
            *response = owner.handleSessionRequest(request);
            completed->signal();
        });

        if (dispatched) {
            if (!completed->wait(30000)) {
                auto error = PresetJson::object();
                error->setProperty("ok", false);
                error->setProperty("message", "Timed out waiting for message thread");
                *response = PresetJson::toVar(error);
            }
        } else {
            auto error = PresetJson::object();
            error->setProperty("ok", false);
            error->setProperty("message", "Could not dispatch session request to message thread");
            *response = PresetJson::toVar(error);
        }

        String responseText = JSON::toString(*response, true) + "\n";
        CharPointer_UTF8 utf8 = responseText.toUTF8();
        ::write(clientFd, utf8.getAddress(), std::strlen(utf8.getAddress()));
      #else
        ignoreUnused(clientFd);
      #endif
    }

    CycleAutomation& owner;
    String socketPath;
    int serverFd{-1};
};

CycleAutomation::Options CycleAutomation::parseOptions(const String& commandLine) {
    Options parsed;
    StringArray tokens;
    tokens.addTokens(commandLine, true);
    tokens.trim();
    tokens.removeEmptyStrings();

    for (int i = 0; i < tokens.size(); ++i) {
        const String& token = tokens[i];

        if (token == "--agent-script" && i + 1 < tokens.size()) {
            parsed.hasScript = true;
            parsed.scriptPath = tokens[++i].unquoted();
        } else if (token.startsWith("--agent-script=")) {
            parsed.hasScript = true;
            parsed.scriptPath = token.fromFirstOccurrenceOf("=", false, false).unquoted();
        } else if (token == "--agent-report" && i + 1 < tokens.size()) {
            parsed.reportPath = tokens[++i].unquoted();
        } else if (token.startsWith("--agent-report=")) {
            parsed.reportPath = token.fromFirstOccurrenceOf("=", false, false).unquoted();
        } else if (token == "--agent-session" && i + 1 < tokens.size()) {
            parsed.hasSession = true;
            parsed.sessionPath = tokens[++i].unquoted();
        } else if (token.startsWith("--agent-session=")) {
            parsed.hasSession = true;
            parsed.sessionPath = token.fromFirstOccurrenceOf("=", false, false).unquoted();
        }
    }

    return parsed;
}

String CycleAutomation::stripAutomationArgs(const String& commandLine) {
    StringArray tokens;
    tokens.addTokens(commandLine, true);
    tokens.trim();
    tokens.removeEmptyStrings();

    StringArray kept;

    for (int i = 0; i < tokens.size(); ++i) {
        const String& token = tokens[i];

        if ((token == "--agent-script" || token == "--agent-report" || token == "--agent-session") && i + 1 < tokens.size()) {
            ++i;
            continue;
        }

        if (token.startsWith("--agent-script=") ||
                token.startsWith("--agent-report=") ||
                token.startsWith("--agent-session=")) {
            continue;
        }

        kept.add(token);
    }

    return kept.joinIntoString(" ");
}

void CycleAutomation::beginFromCommandLine(const String& commandLine) {
    options = parseOptions(commandLine);

    if (options.hasSession) {
        startSessionServer();
    }

    if (options.hasScript && !hasRun) {
        startTimer(500);
    }
}

void CycleAutomation::startSessionServer() {
    if (sessionServer != nullptr) {
        return;
    }

    String message;
    sessionServer = std::make_unique<SessionServer>(*this, options.sessionPath);

    if (!sessionServer->start(message)) {
        DBG(message);
        sessionServer = nullptr;
        return;
    }

    DBG(message);
}

void CycleAutomation::appendResult(Array<var>& results, const String& type, bool ok, const String& message, const var& data) {
    results.add(makeResult(type, ok, message, data));
}

void CycleAutomation::timerCallback() {
    stopTimer();
    runScript();
}

void CycleAutomation::runScript() {
    hasRun = true;

    Array<var> results;
    File scriptFile(options.scriptPath);
    bool shouldQuit = true;

    if (!scriptFile.existsAsFile()) {
        appendResult(results, "script", false, "Script file does not exist: " + options.scriptPath);
    } else {
        var script = JSON::parse(scriptFile.loadFileAsString());
        var commandsValue = PresetJson::property(script, "commands");
        const Array<var>* commands = PresetJson::getArray(commandsValue);

        shouldQuit = getBool(script, "quit", true);

        if (commands == nullptr) {
            commands = PresetJson::getArray(script);
        }

        if (commands == nullptr) {
            appendResult(results, "script", false, "Script must be an array or an object with a commands array");
        } else {
            for (const auto& command : *commands) {
                runCommand(command, results);
            }
        }
    }

    auto report = PresetJson::object();
    report->setProperty("script", options.scriptPath);
    report->setProperty("results", var(results));
    report->setProperty("snapshot", snapshotState());

    if (options.reportPath.isNotEmpty()) {
        File reportFile(options.reportPath);
        reportFile.replaceWithText(JSON::toString(PresetJson::toVar(report), true));
    }

    if (shouldQuit) {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }
}

void CycleAutomation::runCommand(const var& command, Array<var>& results) {
    results.add(runCommandResult(command));
}

var CycleAutomation::runCommandResult(const var& command) {
    String type = getString(command, "command", getString(command, "type"));
    String message;
    var data;
    bool ok = false;

    if (type == "action") {
        ok = runTourAction(command, message);
    } else if (type == "screenshot") {
        ok = captureScreenshot(command, message, data);
    } else if (type == "captureAudio") {
        ok = captureAudio(command, message, data);
    } else if (type == "exportState") {
        ok = exportState(command, message);
    } else if (type == "exportPreset") {
        ok = exportPreset(command, message);
    } else if (type == "savePreset") {
        ok = savePreset(command, message, data);
    } else if (type == "openPreset") {
        ok = openPreset(command, message, data);
    } else if (type == "openFactoryPreset") {
        ok = openFactoryPreset(command, message, data);
    } else if (type == "openWave") {
        ok = openWave(command, message, data);
    } else if (type == "listMenus") {
        ok = listMenus(command, message, data);
    } else if (type == "invokeMenuItem") {
        ok = invokeMenuItem(command, message, data);
    } else if (type == "listModMatrixMenu") {
        ok = listModMatrixMenu(command, message, data);
    } else if (type == "invokeModMatrixMenu") {
        ok = invokeModMatrixMenu(command, message, data);
    } else if (type == "listModMatrixDimensionMenu") {
        ok = listModMatrixDimensionMenu(command, message, data);
    } else if (type == "invokeModMatrixDimensionMenu") {
        ok = invokeModMatrixDimensionMenu(command, message, data);
    } else if (type == "listEnvelopeConfigMenu") {
        ok = listEnvelopeConfigMenu(command, message, data);
    } else if (type == "invokeEnvelopeConfigMenu") {
        ok = invokeEnvelopeConfigMenu(command, message, data);
    } else if (type == "listSelectorMenu") {
        ok = listSelectorMenu(command, message, data);
    } else if (type == "invokeSelectorMenu") {
        ok = invokeSelectorMenu(command, message, data);
    } else if (type == "listHoverSelectorMenu") {
        ok = listHoverSelectorMenu(command, message, data);
    } else if (type == "invokeHoverSelectorMenu") {
        ok = invokeHoverSelectorMenu(command, message, data);
    } else if (type == "logMessage") {
        ok = logMessage(command, message, data);
    } else if (type == "openGLDiagnostics") {
        ok = openGLDiagnostics(command, message, data);
    } else if (type == "inspectTargets") {
        ok = inspectTargets(command, message, data);
    } else if (type == "inspectTree") {
        ok = inspectTree(command, message, data);
    } else if (type == "setControl") {
        ok = setControl(command, message, data);
    } else if (type == "resetMainPanelView") {
        ok = resetMainPanelView(command, message, data);
    } else if (type == "dismissTransientUi") {
        ok = dismissTransientUi(command, message, data);
    } else if (type == "setCalloutCollapsed") {
        ok = setCalloutCollapsed(command, message, data);
    } else if (type == "pointer") {
        ok = pointer(command, message, data);
    } else if (type == "assertTarget") {
        ok = assertTarget(command, message, data);
    } else if (type == "assertState") {
        ok = assertState(command, message, data);
    } else if (type == "listAssertionPaths") {
        ok = listAssertionPaths(command, message, data);
    } else if (type == "listMeshTargets") {
        ok = listMeshTargets(command, message, data);
    } else if (type == "exportMeshState") {
        ok = exportMeshState(command, message, data);
    } else if (type == "selectVertex" || type == "addVertex" || type == "moveVertex" || type == "deleteVertex") {
        ok = mutateMeshVertex(command, message, data);
    } else if (type == "meshSelectionGesture") {
        ok = meshSelectionGesture(command, message, data);
    } else if (type == "waitForIdle") {
        ok = waitForIdle(command, message, data);
    } else if (type == "snapshotState") {
        ok = true;
        data = snapshotState();
        message = "Snapshot captured";
    } else if (type == "ping") {
        ok = true;
        message = "pong";
    } else if (type == "quit") {
        ok = true;
        message = "Quit requested";
        MessageManager::callAsync([] {
            JUCEApplication::getInstance()->systemRequestedQuit();
        });
    } else {
        message = "Unknown automation command: " + type;
    }

    return makeResult(type, ok, message, data);
}

var CycleAutomation::handleSessionRequest(const var& request) {
    auto response = PresetJson::object();
    response->setProperty("ok", false);

    if (request.isVoid()) {
        response->setProperty("message", "Invalid JSON request");
        return PresetJson::toVar(response);
    }

    var id = PresetJson::property(request, "id");

    if (!id.isVoid()) {
        response->setProperty("id", id);
    }

    var command = PresetJson::property(request, "command");

    if (command.isVoid()) {
        command = request;
    }

    hasRun = true;

    var result = runCommandResult(command);
    bool ok = PresetJson::boolProperty(result, "ok");
    response->setProperty("ok", ok);
    response->setProperty("result", result);
    return PresetJson::toVar(response);
}

bool CycleAutomation::runTourAction(const var& command, String& message) {
    auto* object = PresetJson::getObject(command);

    if (object == nullptr) {
        message = "Action command must be an object";
        return false;
    }

    XmlElement actionElem("Action");
    const NamedValueSet& properties = object->getProperties();

    for (int i = 0; i < properties.size(); ++i) {
        Identifier name = properties.getName(i);

        String nameString = name.toString();

        if (nameString == "command") {
            continue;
        }

        if (nameString == "type" && properties.getValueAt(i).toString() == "action") {
            continue;
        }

        if (nameString == "actionType") {
            actionElem.setAttribute("type", properties.getValueAt(i).toString());
            continue;
        }

        actionElem.setAttribute(name.toString(), properties.getValueAt(i).toString());
    }

    CycleTour::Action action;
    getObj(CycleTour).readAction(action, &actionElem);
    getObj(CycleTour).performAction(action);

    message = "Action executed";
    return true;
}

bool CycleAutomation::captureScreenshot(const var& command, String& message, var& data) {
    String path = getString(command, "path");

    if (path.isEmpty()) {
        message = "Screenshot command requires path";
        return false;
    }

    bool waitedForIdle = drainMessageLoopIfRequested(command);
    Component* component = resolveComponent(command);

    if (component == nullptr) {
        message = "Screenshot target could not be resolved";
        return false;
    }

    Image image = component->createComponentSnapshot(component->getLocalBounds());

    if (!image.isValid()) {
        message = "Snapshot image is invalid";
        return false;
    }

    File file(path);
    std::unique_ptr<FileOutputStream> stream(file.createOutputStream());

    if (stream == nullptr || !stream->openedOk()) {
        message = "Could not open screenshot path: " + path;
        return false;
    }

    PNGImageFormat format;

    if (!format.writeImageToStream(image, *stream)) {
        message = "Could not encode screenshot: " + path;
        return false;
    }

    auto json = PresetJson::object();
    json->setProperty("path", path);
    json->setProperty("width", image.getWidth());
    json->setProperty("height", image.getHeight());
    json->setProperty("area", getString(command, "area"));
    json->setProperty("target", getString(command, "target"));
    json->setProperty("waitedForIdle", waitedForIdle);
    json->setProperty("localBounds", rectangleState(component->getLocalBounds()));
    json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));

    data = PresetJson::toVar(json);
    message = "Screenshot written: " + path;
    return true;
}

bool CycleAutomation::captureAudio(const var& command, String& message, var& data) {
    AudioHub& audioHub = getObj(AudioHub);
    AudioSourceProcessor* processor = audioHub.getAudioSourceProcessor();

    if (processor == nullptr) {
        message = "No audio source processor is active";
        return false;
    }

    const double originalSampleRate = audioHub.getSampleRate();
    const int originalBufferSize = audioHub.getBufferSize();
    const double sampleRate = getDouble(command, "sampleRate", originalSampleRate > 0.0 ? originalSampleRate : 44100.0);
    const int blockSize = jlimit(16, 8192, int(getDouble(command, "blockSize", originalBufferSize > 0 ? originalBufferSize : 512)));
    const int channels = jlimit(1, 8, int(getDouble(command, "channels", 2.0)));
    const double durationMs = jlimit(1.0, 60000.0, getDouble(command, "durationMs", 1000.0));
    const int totalSamples = jmax(1, int(std::round(durationMs * sampleRate / 1000.0)));
    const String path = getString(command, "path");
    Array<ScheduledMidiEvent> midiEvents;

    if (!buildMidiSchedule(command, midiEvents, sampleRate, totalSamples, message)) {
        return false;
    }

  #if ! PLUGIN_MODE
    audioHub.suspendAudio();
  #endif

    struct RestoreAudioState {
        AudioHub& audioHub;
        double sampleRate;
        int bufferSize;

        ~RestoreAudioState() {
            if (sampleRate > 0.0 && bufferSize > 0) {
                audioHub.prepareToPlay(bufferSize, sampleRate);
            }

          #if ! PLUGIN_MODE
            audioHub.resumeAudio();
          #endif
        }
    } restoreAudioState{ audioHub, originalSampleRate, originalBufferSize };

    audioHub.resetKeyboardState();
    audioHub.prepareToPlay(blockSize, sampleRate);

    AudioSampleBuffer capture(channels, totalSamples);
    AudioSampleBuffer block(channels, blockSize);
    int eventIndex = 0;

    for (int startSample = 0; startSample < totalSamples; startSample += blockSize) {
        int blockSamples = jmin(blockSize, totalSamples - startSample);
        MidiBuffer midiBuffer;
        block.setSize(channels, blockSamples, false, false, true);
        block.clear();

        while (eventIndex < midiEvents.size() && midiEvents.getReference(eventIndex).sampleOffset < startSample) {
            ++eventIndex;
        }

        for (int i = eventIndex; i < midiEvents.size(); ++i) {
            const ScheduledMidiEvent& event = midiEvents.getReference(i);

            if (event.sampleOffset >= startSample + blockSamples) {
                break;
            }

            midiBuffer.addEvent(event.message, event.sampleOffset - startSample);
        }

        processor->processBlock(block, midiBuffer);

        for (int ch = 0; ch < channels; ++ch) {
            capture.copyFrom(ch, startSample, block, ch, 0, blockSamples);
        }
    }

    data = audioCaptureMetrics(capture, sampleRate);
    auto* dataObject = PresetJson::getObject(data);

    if (dataObject != nullptr) {
        dataObject->setProperty("path", path);
        dataObject->setProperty("events", midiEvents.size());
        dataObject->setProperty("blockSize", blockSize);
    }

    if (path.isNotEmpty()) {
        File file(path);
        file.getParentDirectory().createDirectory();
        std::unique_ptr<FileOutputStream> stream(file.createOutputStream());

        if (stream == nullptr || !stream->openedOk()) {
            message = "Could not open audio capture path: " + path;
            return false;
        }

        WavAudioFormat wavFormat;
        std::unique_ptr<AudioFormatWriter> writer(
            wavFormat.createWriterFor(stream.get(), sampleRate, uint32(channels), 24, {}, 0));

        if (writer == nullptr) {
            message = "Could not create WAV writer: " + path;
            return false;
        }

        stream.release();

        if (!writer->writeFromAudioSampleBuffer(capture, 0, totalSamples)) {
            message = "Could not write WAV capture: " + path;
            return false;
        }
    }

    if (!checkAudioThresholds(command, data, message)) {
        return false;
    }

    message = path.isNotEmpty() ? "Audio captured: " + path : "Audio captured";
    return true;
}

bool CycleAutomation::exportState(const var& command, String& message) {
    String requestedSource = getString(command, "source", getString(command, "target"));
    String source = resolveSavableSourceName(requestedSource);
    String jsonPath = getString(command, "jsonPath", getString(command, "statePath"));
    String path = getString(command, "path");

    if (requestedSource.isEmpty()) {
        message = "Export command requires source";
        return false;
    }

    var exported = getObj(Document).exportSavableJSON(source);

    if (exported.isVoid()) {
        message = "No savable source found: " + source;
        return false;
    }

    if (jsonPath.isNotEmpty()) {
        var selected;

        if (!getPathValue(exported, jsonPath, selected)) {
            message = "Export JSON path not found: " + jsonPath;
            return false;
        }

        exported = selected;
    }

    String json = JSON::toString(exported, true);

    if (path.isNotEmpty()) {
        File(path).replaceWithText(json);
        message = "State exported: " + path;
    } else {
        message = json;
    }

    return true;
}

bool CycleAutomation::exportPreset(const var& command, String& message) {
    String path = getString(command, "path");
    String jsonPath = getString(command, "jsonPath", getString(command, "statePath"));
    var exported = getObj(Document).exportPresetJSON();

    if (jsonPath.isNotEmpty()) {
        var selected;

        if (!getPathValue(exported, jsonPath, selected)) {
            message = "Preset JSON path not found: " + jsonPath;
            return false;
        }

        exported = selected;
    }

    String json = JSON::toString(exported, true);

    if (path.isNotEmpty()) {
        File(path).replaceWithText(json);
        message = "Preset JSON exported: " + path;
    } else {
        message = json;
    }

    return true;
}

bool CycleAutomation::savePreset(const var& command, String& message, var& data) {
    String path = getString(command, "path");

    if (path.isEmpty()) {
        message = "savePreset requires path";
        return false;
    }

    File file(path);
    getObj(Document).save(path);

    auto json = PresetJson::object();
    json->setProperty("path", path);
    json->setProperty("exists", file.existsAsFile());
    json->setProperty("sizeBytes", file.existsAsFile() ? file.getSize() : 0);
    data = PresetJson::toVar(json);

    if (!file.existsAsFile()) {
        message = "Preset was not written: " + path;
        return false;
    }

    message = "Preset saved: " + path;
    return true;
}

bool CycleAutomation::openPreset(const var& command, String& message, var& data) {
    String path = getString(command, "path");

    if (path.isEmpty()) {
        message = "openPreset requires path";
        return false;
    }

    File file(path);

    if (!file.existsAsFile()) {
        message = "Preset file does not exist: " + path;
        return false;
    }

    bool opened = getObj(Document).open(path);

    auto json = PresetJson::object();
    json->setProperty("path", path);
    json->setProperty("opened", opened);
    json->setProperty("document", getObj(Document).getDocumentName());
    data = PresetJson::toVar(json);

    if (!opened) {
        message = "Preset could not be opened: " + path;
        return false;
    }

    message = "Preset opened: " + path;
    return true;
}

bool CycleAutomation::openFactoryPreset(const var& command, String& message, var& data) {
    String preset = getString(command, "preset", getString(command, "name"));

    if (preset.isEmpty()) {
        message = "openFactoryPreset requires preset";
        return false;
    }

    File file = getObj(FileManager).findFactoryPresetFile(preset);

    if (!file.existsAsFile()) {
        message = "Factory preset file does not exist: " + preset;
        return false;
    }

    getObj(FileManager).openFactoryPreset(preset);

    auto json = PresetJson::object();
    json->setProperty("preset", preset);
    json->setProperty("path", file.getFullPathName());
    json->setProperty("document", getObj(Document).getDocumentName());
    data = PresetJson::toVar(json);

    message = "Factory preset opened: " + preset;
    return true;
}

bool CycleAutomation::openWave(const var& command, String& message, var& data) {
    String path = getString(command, "path");
    String invokerName = getString(command, "invoker", "dialog");
    int defaultNote = getInt(command, "defaultNote", -1);

    if (path.isEmpty()) {
        message = "openWave requires path";
        return false;
    }

    File file(path);

    if (! File::isAbsolutePath(path)) {
        file = File(String(CYCLE_SOURCE_DIR)).getChildFile(path);
    }

    if (!file.exists()) {
        message = "Wave path does not exist: " + path;
        return false;
    }

    Dialogs::OpenWaveInvoker invoker = invokerName == "preset" ? Dialogs::PresetSource
                                                               : Dialogs::DialogSource;
    bool opened = false;

    if (invoker == Dialogs::DialogSource && defaultNote < 0) {
        Dialogs::WaveOpenData waveData(&getObj(Dialogs),
                                       nullptr,
                                       DialogActions::TrackPitchAction,
                                       Dialogs::DoNothing);
        waveData.audioFile = file;
        Dialogs::openWaveCallback(Dialogs::DialogSave, waveData);
        opened = getSetting(WaveLoaded) != 0;
    } else {
        opened = getObj(FileManager).openWave(file, invoker, defaultNote);
    }

    drainMessageLoopIfRequested(command);
    auto& meshLibrary = getObj(MeshLibrary);
    auto& wavePitchGroup = meshLibrary.getLayerGroup(LayerGroups::GroupWavePitch);
    auto json = PresetJson::object();

    json->setProperty("path", file.getFullPathName());
    json->setProperty("invoker", invokerName);
    json->setProperty("opened", opened);
    json->setProperty("drawWave", getSetting(DrawWave) != 0);
    json->setProperty("waveLoaded", getSetting(WaveLoaded) != 0);
    json->setProperty("wavePitchLayerCount", wavePitchGroup.size());
    json->setProperty("wavePitchCurrentIndex", wavePitchGroup.current);
    data = PresetJson::toVar(json);

    if (!opened) {
        message = "Wave could not be opened: " + path;
        return false;
    }

    message = "Wave opened: " + file.getFullPathName();
    return true;
}

bool CycleAutomation::listMenus(const var& command, String& message, var& data) {
    SynthMenuBarModel& model = getObj(SynthMenuBarModel);
    int requestedMenu = commandMenuIndex(model, command);
    StringArray names = model.getMenuBarNames();
    Array<var> menus;
    Array<var> items;

    for (int i = 0; i < names.size(); ++i) {
        if (requestedMenu >= 0 && requestedMenu != i) {
            continue;
        }

        PopupMenu menu = model.getMenuForIndex(i, names[i]);

        auto menuJson = PresetJson::object();
        menuJson->setProperty("name", names[i]);
        menuJson->setProperty("menuIndex", i);
        menuJson->setProperty("itemCount", menu.getNumItems());
        menuJson->setProperty("hasActiveItems", menu.containsAnyActiveItems());
        menus.add(PresetJson::toVar(menuJson));

        appendMenuItems(items, menu, names[i], i, {});
    }

    if (requestedMenu >= names.size()) {
        message = "Menu index out of range: " + String(requestedMenu);
        return false;
    }

    if (requestedMenu < 0 && getString(command, "menu").isNotEmpty()) {
        message = "Menu not found: " + getString(command, "menu");
        return false;
    }

    auto json = PresetJson::object();
    json->setProperty("menus", var(menus));
    json->setProperty("items", var(items));
    json->setProperty("menuCount", menus.size());
    json->setProperty("itemCount", items.size());
    data = PresetJson::toVar(json);

    message = "Listed " + String(items.size()) + " menu items";
    return true;
}

bool CycleAutomation::invokeMenuItem(const var& command, String& message, var& data) {
    SynthMenuBarModel& model = getObj(SynthMenuBarModel);
    int menuIndex = commandMenuIndex(model, command);
    String menuName = menuNameForIndex(model, menuIndex);

    if (menuIndex < 0 || menuName.isEmpty()) {
        message = "Menu could not be resolved";
        return false;
    }

    PopupMenu menu = model.getMenuForIndex(menuIndex, menuName);
    PopupMenu::Item item;
    StringArray path = commandMenuPath(command);
    var itemIdVar = PresetJson::property(command, "itemId");

    if (itemIdVar.isVoid()) {
        itemIdVar = PresetJson::property(command, "id");
    }

    bool found = false;

    if (!itemIdVar.isVoid()) {
        found = findMenuItemById(menu, int(itemIdVar), item);
    } else if (!path.isEmpty()) {
        found = findMenuItemByPath(menu, path, 0, item);
    }

    if (!found) {
        message = "Menu item could not be resolved";
        return false;
    }

    auto json = PresetJson::object();
    json->setProperty("menu", menuName);
    json->setProperty("menuIndex", menuIndex);
    json->setProperty("text", item.text);
    json->setProperty("itemId", item.itemID);
    json->setProperty("enabled", item.isEnabled);
    json->setProperty("ticked", item.isTicked);
    json->setProperty("path", pathToVar(path));

    if (!item.isEnabled && !getBool(command, "allowDisabled")) {
        data = PresetJson::toVar(json);
        message = "Menu item is disabled: " + item.text;
        return false;
    }

    if (item.itemID == 0) {
        data = PresetJson::toVar(json);
        message = "Menu item is not triggerable: " + item.text;
        return false;
    }

    bool waitedForIdle = drainMessageLoopIfRequested(command);
    model.menuItemSelected(item.itemID, menuIndex);
    drainMessageLoopIfRequested(command);

    json->setProperty("waitedForIdle", waitedForIdle);
    data = PresetJson::toVar(json);
    message = "Menu item invoked: " + item.text;
    return true;
}

bool CycleAutomation::listModMatrixMenu(const var& command, String& message, var& data) {
    auto& panel = getObj(ModMatrixPanel);
    String kind = modMatrixMenuKind(command);

    if (kind.isEmpty()) {
        message = "Mod matrix menu requires menu/kind input or output";
        return false;
    }

    PopupMenu menu = modMatrixMenuForKind(panel, kind);
    Array<var> items;
    appendMenuItems(items, menu, "ModMatrix:" + kind, kind == "input" ? 0 : 1, {});

    auto json = PresetJson::object();
    json->setProperty("menu", kind);
    json->setProperty("itemCount", items.size());
    json->setProperty("items", var(items));
    json->setProperty("inputCount", panel.inputs.size());
    json->setProperty("outputCount", panel.outputs.size());
    json->setProperty("mappingCount", panel.mappings.size());
    data = PresetJson::toVar(json);

    message = "Listed " + String(items.size()) + " modulation matrix " + kind + " menu items";
    return true;
}

bool CycleAutomation::invokeModMatrixMenu(const var& command, String& message, var& data) {
    auto& panel = getObj(ModMatrixPanel);
    String kind = modMatrixMenuKind(command);

    if (kind.isEmpty()) {
        message = "Mod matrix menu requires menu/kind input or output";
        return false;
    }

    PopupMenu menu = modMatrixMenuForKind(panel, kind);
    PopupMenu::Item item;
    StringArray path = commandMenuPath(command);
    var itemIdVar = PresetJson::property(command, "itemId");

    if (itemIdVar.isVoid()) {
        itemIdVar = PresetJson::property(command, "id");
    }

    bool found = false;

    if (!itemIdVar.isVoid()) {
        found = findMenuItemById(menu, int(itemIdVar), item);
    } else if (!path.isEmpty()) {
        found = findMenuItemByPath(menu, path, 0, item);
    }

    if (!found) {
        message = "Mod matrix menu item could not be resolved";
        return false;
    }

    auto json = PresetJson::object();
    json->setProperty("menu", kind);
    json->setProperty("text", item.text);
    json->setProperty("itemId", item.itemID);
    json->setProperty("enabled", item.isEnabled);
    json->setProperty("path", pathToVar(path));

    if (!item.isEnabled && !getBool(command, "allowDisabled")) {
        data = PresetJson::toVar(json);
        message = "Mod matrix menu item is disabled: " + item.text;
        return false;
    }

    if (item.itemID == 0) {
        data = PresetJson::toVar(json);
        message = "Mod matrix menu item is not triggerable: " + item.text;
        return false;
    }

    bool waitedForIdle = drainMessageLoopIfRequested(command);

    if (kind == "input") {
        panel.addInput(item.itemID);
    } else {
        panel.addDestination(item.itemID);
    }

    panel.selfSize();
    drainMessageLoopIfRequested(command);

    json->setProperty("waitedForIdle", waitedForIdle);
    json->setProperty("inputCount", panel.inputs.size());
    json->setProperty("outputCount", panel.outputs.size());
    json->setProperty("mappingCount", panel.mappings.size());
    data = PresetJson::toVar(json);

    message = "Mod matrix " + kind + " menu item invoked: " + item.text;
    return true;
}

bool CycleAutomation::listModMatrixDimensionMenu(const var& command, String& message, var& data) {
    auto& panel = getObj(ModMatrixPanel);
    int inputId = int(PresetJson::property(command, "inputId"));
    int outputId = int(PresetJson::property(command, "outputId"));
    int mappingIndex = panel.indexOfMapping(inputId, outputId);
    int oldDim = mappingIndex < 0 ? ModMatrixPanel::NullDim : panel.mappings.getReference(mappingIndex).dim;
    bool active = oldDim != ModMatrixPanel::YellowDim;
    active &= !(inputId == ModMatrixPanel::VoiceTime && outputId >= ModMatrixPanel::VolEnvId);

    Array<var> items;
    appendModMatrixDimensionItems(items, inputId, outputId, oldDim, active);

    auto json = PresetJson::object();
    json->setProperty("inputId", inputId);
    json->setProperty("outputId", outputId);
    json->setProperty("mappingIndex", mappingIndex);
    json->setProperty("currentDimension", oldDim);
    json->setProperty("active", active);
    json->setProperty("itemCount", items.size());
    json->setProperty("items", var(items));
    data = PresetJson::toVar(json);

    message = "Listed modulation matrix dimension menu";
    return true;
}

bool CycleAutomation::invokeModMatrixDimensionMenu(const var& command, String& message, var& data) {
    auto& panel = getObj(ModMatrixPanel);
    int inputId = int(PresetJson::property(command, "inputId"));
    int outputId = int(PresetJson::property(command, "outputId"));
    int newDim = modMatrixDimensionForCommand(command);
    int mappingIndex = panel.indexOfMapping(inputId, outputId);
    int oldDim = mappingIndex < 0 ? ModMatrixPanel::NullDim : panel.mappings.getReference(mappingIndex).dim;
    bool active = oldDim != ModMatrixPanel::YellowDim;
    active &= !(inputId == ModMatrixPanel::VoiceTime && outputId >= ModMatrixPanel::VolEnvId);

    if (!active && !getBool(command, "allowInactive")) {
        message = "Mod matrix dimension menu is inactive for this cell";
        return false;
    }

    if (newDim != ModMatrixPanel::NullDim && newDim != ModMatrixPanel::RedDim && newDim != ModMatrixPanel::BlueDim) {
        message = "Mod matrix dimension must be none, red, or blue";
        return false;
    }

    if (oldDim != newDim) {
        panel.mappingChanged(mappingIndex, inputId, outputId, oldDim, newDim);
    }

    auto json = PresetJson::object();
    json->setProperty("inputId", inputId);
    json->setProperty("outputId", outputId);
    json->setProperty("oldDimension", oldDim);
    json->setProperty("newDimension", newDim);
    json->setProperty("mappingCount", panel.mappings.size());
    data = PresetJson::toVar(json);

    message = "Mod matrix dimension menu item invoked";
    return true;
}

bool CycleAutomation::listEnvelopeConfigMenu(const var& command, String& message, var& data) {
    int currentGroup = getObj(EnvelopeInter2D).getLayerType();
    int groupId = envelopeGroupForCommand(command, currentGroup);
    MeshLibrary::EnvProps* props = getObj(MeshLibrary).getCurrentEnvProps(groupId);

    if (props == nullptr) {
        message = "Envelope config menu requires a valid envelope group";
        return false;
    }

    PopupMenu menu = envelopeConfigMenuForProps(groupId, *props);
    Array<var> items;
    appendMenuItems(items, menu, "EnvelopeConfig", 0, {});

    auto propsState = envelopeConfigPropsState(groupId, *props);
    auto json = PresetJson::object();
    json->setProperty("groupId", groupId);
    json->setProperty("group", PresetJson::property(propsState, "group"));
    json->setProperty("currentGroupId", currentGroup);
    json->setProperty("itemCount", items.size());
    json->setProperty("items", var(items));
    json->setProperty("props", propsState);
    data = PresetJson::toVar(json);

    message = "Listed envelope config menu";
    return true;
}

bool CycleAutomation::invokeEnvelopeConfigMenu(const var& command, String& message, var& data) {
    auto& interactor = getObj(EnvelopeInter2D);
    int currentGroup = interactor.getLayerType();
    int groupId = envelopeGroupForCommand(command, currentGroup);
    MeshLibrary::EnvProps* props = getObj(MeshLibrary).getCurrentEnvProps(groupId);

    if (props == nullptr) {
        message = "Envelope config menu requires a valid envelope group";
        return false;
    }

    PopupMenu menu = envelopeConfigMenuForProps(groupId, *props);
    PopupMenu::Item item;
    StringArray path = commandMenuPath(command);
    var itemIdVar = PresetJson::property(command, "itemId");

    if (itemIdVar.isVoid()) {
        itemIdVar = PresetJson::property(command, "id");
    }

    bool found = false;

    if (!itemIdVar.isVoid()) {
        found = findMenuItemById(menu, int(itemIdVar), item);
    } else if (!path.isEmpty()) {
        found = findMenuItemByPath(menu, path, 0, item);
    }

    if (!found) {
        message = "Envelope config menu item could not be resolved";
        return false;
    }

    auto json = PresetJson::object();
    json->setProperty("groupId", groupId);
    json->setProperty("currentGroupId", currentGroup);
    json->setProperty("text", item.text);
    json->setProperty("itemId", item.itemID);
    json->setProperty("enabled", item.isEnabled);
    json->setProperty("tickedBefore", item.isTicked);
    json->setProperty("path", pathToVar(path));
    json->setProperty("before", envelopeConfigPropsState(groupId, *props));

    if (!item.isEnabled && !getBool(command, "allowDisabled")) {
        data = PresetJson::toVar(json);
        message = "Envelope config menu item is disabled: " + item.text;
        return false;
    }

    if (item.itemID == 0) {
        data = PresetJson::toVar(json);
        message = "Envelope config menu item is not triggerable: " + item.text;
        return false;
    }

    bool waitedForIdle = drainMessageLoopIfRequested(command);
    interactor.chooseConfigScale(item.itemID, props);
    drainMessageLoopIfRequested(command);

    json->setProperty("waitedForIdle", waitedForIdle);
    json->setProperty("after", envelopeConfigPropsState(groupId, *props));
    data = PresetJson::toVar(json);

    message = "Envelope config menu item invoked: " + item.text;
    return true;
}

bool CycleAutomation::listSelectorMenu(const var& command, String& message, var& data) {
    auto* selector = dynamic_cast<SelectorPanel*>(resolveComponent(command));

    if (selector == nullptr) {
        message = "Selector menu requires an area/target resolving to SelectorPanel";
        return false;
    }

    PopupMenu menu = selectorMenuForPanel(*selector);
    Array<var> items;
    appendMenuItems(items, menu, "SelectorPanel", 0, {});

    auto json = PresetJson::object();
    json->setProperty("area", getString(command, "area"));
    json->setProperty("target", getString(command, "target"));
    json->setProperty("itemCount", items.size());
    json->setProperty("items", var(items));
    json->setProperty("selector", selectorState(*selector));
    data = PresetJson::toVar(json);

    message = "Listed selector menu";
    return true;
}

bool CycleAutomation::invokeSelectorMenu(const var& command, String& message, var& data) {
    auto* selector = dynamic_cast<SelectorPanel*>(resolveComponent(command));

    if (selector == nullptr) {
        message = "Selector menu requires an area/target resolving to SelectorPanel";
        return false;
    }

    PopupMenu menu = selectorMenuForPanel(*selector);
    PopupMenu::Item item;
    StringArray path = commandMenuPath(command);
    var itemIdVar = PresetJson::property(command, "itemId");

    if (itemIdVar.isVoid()) {
        itemIdVar = PresetJson::property(command, "id");
    }

    bool found = false;

    if (!itemIdVar.isVoid()) {
        found = findMenuItemById(menu, int(itemIdVar), item);
    } else if (!path.isEmpty()) {
        found = findMenuItemByPath(menu, path, 0, item);
    }

    if (!found) {
        message = "Selector menu item could not be resolved";
        return false;
    }

    auto json = PresetJson::object();
    json->setProperty("area", getString(command, "area"));
    json->setProperty("target", getString(command, "target"));
    json->setProperty("text", item.text);
    json->setProperty("itemId", item.itemID);
    json->setProperty("enabled", item.isEnabled);
    json->setProperty("tickedBefore", item.isTicked);
    json->setProperty("path", pathToVar(path));
    json->setProperty("before", selectorState(*selector));

    if (!item.isEnabled && !getBool(command, "allowDisabled")) {
        data = PresetJson::toVar(json);
        message = "Selector menu item is disabled: " + item.text;
        return false;
    }

    if (item.itemID == 0) {
        data = PresetJson::toVar(json);
        message = "Selector menu item is not triggerable: " + item.text;
        return false;
    }

    bool waitedForIdle = drainMessageLoopIfRequested(command);
    selector->clickedOnRow(item.itemID - 1);
    drainMessageLoopIfRequested(command);

    json->setProperty("waitedForIdle", waitedForIdle);
    json->setProperty("after", selectorState(*selector));
    data = PresetJson::toVar(json);

    message = "Selector menu item invoked: " + item.text;
    return true;
}

bool CycleAutomation::listHoverSelectorMenu(const var& command, String& message, var& data) {
    auto* selector = dynamic_cast<HoverSelector*>(resolveComponent(command));

    if (selector == nullptr) {
        message = "Hover selector menu requires an area/target resolving to HoverSelector";
        return false;
    }

    selector->prepareForPopup();

    Array<var> items;
    appendMenuItems(items, selector->menu, "HoverSelector", 0, {});

    auto json = PresetJson::object();
    json->setProperty("area", getString(command, "area"));
    json->setProperty("target", getString(command, "target"));
    json->setProperty("itemCount", items.size());
    json->setProperty("items", var(items));
    json->setProperty("selector", hoverSelectorState(*selector));
    data = PresetJson::toVar(json);

    message = "Listed hover selector menu";
    return true;
}

bool CycleAutomation::invokeHoverSelectorMenu(const var& command, String& message, var& data) {
    auto* selector = dynamic_cast<HoverSelector*>(resolveComponent(command));

    if (selector == nullptr) {
        message = "Hover selector menu requires an area/target resolving to HoverSelector";
        return false;
    }

    selector->prepareForPopup();

    PopupMenu::Item item;
    StringArray path = commandMenuPath(command);
    var itemIdVar = PresetJson::property(command, "itemId");

    if (itemIdVar.isVoid()) {
        itemIdVar = PresetJson::property(command, "id");
    }

    bool found = false;

    if (!itemIdVar.isVoid()) {
        found = findMenuItemById(selector->menu, int(itemIdVar), item);
    } else if (!path.isEmpty()) {
        found = findMenuItemByPath(selector->menu, path, 0, item);
    }

    if (!found) {
        message = "Hover selector menu item could not be resolved";
        return false;
    }

    auto json = PresetJson::object();
    json->setProperty("area", getString(command, "area"));
    json->setProperty("target", getString(command, "target"));
    json->setProperty("text", item.text);
    json->setProperty("itemId", item.itemID);
    json->setProperty("enabled", item.isEnabled);
    json->setProperty("tickedBefore", item.isTicked);
    json->setProperty("selection", selector->itemIsSelection(item.itemID));
    json->setProperty("path", pathToVar(path));
    json->setProperty("before", hoverSelectorState(*selector));

    if (!item.isEnabled && !getBool(command, "allowDisabled")) {
        data = PresetJson::toVar(json);
        message = "Hover selector menu item is disabled: " + item.text;
        return false;
    }

    if (!selector->itemIsSelection(item.itemID) && !getBool(command, "allowNonSelection")) {
        data = PresetJson::toVar(json);
        message = "Hover selector menu item is not a selection: " + item.text;
        return false;
    }

    if (item.itemID == 0) {
        data = PresetJson::toVar(json);
        message = "Hover selector menu item is not triggerable: " + item.text;
        return false;
    }

    bool waitedForIdle = drainMessageLoopIfRequested(command);
    selector->setSelectedId(item.itemID);
    drainMessageLoopIfRequested(command);

    json->setProperty("waitedForIdle", waitedForIdle);
    json->setProperty("after", hoverSelectorState(*selector));
    data = PresetJson::toVar(json);

    message = "Hover selector menu item invoked: " + item.text;
    return true;
}

bool CycleAutomation::logMessage(const var& command, String& message, var& data) {
    String text = getString(command, "message");

    if (text.isEmpty()) {
        message = "Automation log message is empty";
        return false;
    }

    String line = "CycleAutomation: " + text;
    Logger::writeToLog(line);

    auto json = PresetJson::object();
    json->setProperty("message", text);
    data = PresetJson::toVar(json);

    message = "Automation log message written";
    return true;
}

bool CycleAutomation::openGLDiagnostics(const var& command, String& message, var& data) {
    String requestedArea = getString(command, "area");
    String requestedTarget = getString(command, "target");
    bool pollNow = getBool(command, "poll", true);
    String phase = getString(command, "phase", "automation");
    Array<var> panels;
    std::set<Component*> seen;
    CycleTour& tour = const_cast<CycleTour&>(getObj(CycleTour));

    auto addComponent = [&](Component* component, const String& area, const String& target) {
        if (component == nullptr || seen.count(component) != 0) {
            return;
        }

        seen.insert(component);

        if (auto* openGL = dynamic_cast<OpenGLBase*>(component)) {
            auto json = PresetJson::object();
            json->setProperty("area", area);
            json->setProperty("target", target);
            json->setProperty("class", String(typeid(*component).name()));
            json->setProperty("localBounds", rectangleState(component->getLocalBounds()));
            json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));
            json->setProperty("visible", component->isVisible());
            json->setProperty("showing", component->isShowing());
            json->setProperty("openGL", openGLDiagnosticsState(*openGL, pollNow, phase));
            panels.add(PresetJson::toVar(json));
        }
    };

    if (requestedArea.isNotEmpty()) {
        Component* component = resolveComponent(command);

        if (component == nullptr) {
            message = "OpenGL diagnostic target could not be resolved";
            return false;
        }

        addComponent(component, requestedArea, requestedTarget);
    } else {
        for (auto* area : kInspectableAreas) {
            addComponent(tour.getComponent(area, {}), area, {});
        }

        for (auto* area : kTourGuideAreas) {
            for (auto* target : kInspectableTargets) {
                addComponent(tour.getComponent(area, target), area, target);
            }
        }
    }

    auto json = PresetJson::object();
    json->setProperty("poll", pollNow);
    json->setProperty("phase", phase);
    json->setProperty("panels", var(panels));
    json->setProperty("count", panels.size());
    data = PresetJson::toVar(json);

    message = "OpenGL diagnostics captured for " + String(panels.size()) + " panels";
    return true;
}

bool CycleAutomation::inspectTargets(const var& command, String& message, var& data) {
    String requestedArea = getString(command, "area");
    String requestedTarget = getString(command, "target");
    Array<var> targets;

    if (requestedArea.isNotEmpty()) {
        if (requestedTarget.isNotEmpty()) {
            targets.add(componentState(resolveComponent(command), requestedArea, requestedTarget));
        } else {
            targets.add(componentState(resolveComponent(command), requestedArea, {}));
        }
    } else {
        for (auto* area : kInspectableAreas) {
            targets.add(componentState(getObj(CycleTour).getComponent(area, {}), area, {}));
        }

        for (auto* area : kTourGuideAreas) {
            for (auto* target : kInspectableTargets) {
                Component* component = getObj(CycleTour).getComponent(area, target);

                if (component != nullptr) {
                    targets.add(componentState(component, area, target));
                }
            }
        }
    }

    auto json = PresetJson::object();
    json->setProperty("targets", var(targets));
    json->setProperty("count", targets.size());
    data = PresetJson::toVar(json);
    message = "Inspected " + String(targets.size()) + " targets";

    return true;
}

bool CycleAutomation::inspectTree(const var& command, String& message, var& data) {
    Component* root = resolveComponent(command);
    String requestedArea = getString(command, "area");

    if (root == nullptr) {
        message = "Tree root could not be resolved";
        return false;
    }

    int maxDepth = jlimit(0, 16, int(getDouble(command, "maxDepth", 5.0)));
    int maxNodes = jlimit(1, 5000, int(getDouble(command, "maxNodes", 500.0)));
    bool includeInvisible = getBool(command, "includeInvisible");
    int nodeCount = 0;

    auto json = PresetJson::object();
    json->setProperty("root", componentTreeState(root, "root", 0, maxDepth, maxNodes, nodeCount, includeInvisible));
    json->setProperty("registeredTargets", registeredTargetsForRoot(root, requestedArea));
    json->setProperty("count", nodeCount);
    json->setProperty("maxDepth", maxDepth);
    json->setProperty("maxNodes", maxNodes);
    json->setProperty("truncated", nodeCount >= maxNodes);

    data = PresetJson::toVar(json);
    message = "Inspected component tree with " + String(nodeCount) + " nodes";
    return true;
}

bool CycleAutomation::setControl(const var& command, String& message, var& data) {
    Component* component = resolveComponent(command);

    if (component == nullptr) {
        message = "Control target could not be resolved";
        return false;
    }

    NotificationType notification = getNotificationType(command);

    if (auto* slider = dynamic_cast<Slider*>(component)) {
        var valueVar = PresetJson::property(command, "value");

        if (valueVar.isVoid()) {
            message = "Slider control requires value";
            return false;
        }

        slider->setValue(getDouble(command, "value"), notification);
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        message = "Slider value set";
        return true;
    }

    if (auto* button = dynamic_cast<Button*>(component)) {
        if (getBool(command, "click")) {
            button->triggerClick();
        } else {
            var stateVar = PresetJson::property(command, "toggleState");

            if (stateVar.isVoid()) {
                stateVar = PresetJson::property(command, "value");
            }

            if (stateVar.isVoid()) {
                message = "Button control requires click, toggleState, or value";
                return false;
            }

            button->setToggleState(bool(stateVar), notification);
        }

        data = componentState(component, getString(command, "area"), getString(command, "target"));
        message = "Button control set";
        return true;
    }

    if (auto* comboBox = dynamic_cast<ComboBox*>(component)) {
        var selectedId = PresetJson::property(command, "selectedId");
        var selectedIndex = PresetJson::property(command, "selectedIndex");
        String text = getString(command, "text");

        if (!selectedId.isVoid()) {
            comboBox->setSelectedId(int(selectedId), notification);
        } else if (!selectedIndex.isVoid()) {
            comboBox->setSelectedItemIndex(int(selectedIndex), notification);
        } else if (text.isNotEmpty()) {
            int matchingId = 0;

            for (int i = 0; i < comboBox->getNumItems(); ++i) {
                if (comboBox->getItemText(i) == text) {
                    matchingId = comboBox->getItemId(i);
                    break;
                }
            }

            if (matchingId == 0) {
                message = "ComboBox item text was not found: " + text;
                data = componentState(component, getString(command, "area"), getString(command, "target"));
                return false;
            }

            comboBox->setSelectedId(matchingId, notification);
        } else {
            message = "ComboBox control requires selectedId, selectedIndex, or text";
            data = componentState(component, getString(command, "area"), getString(command, "target"));
            return false;
        }

        data = componentState(component, getString(command, "area"), getString(command, "target"));
        message = "ComboBox control set";
        return true;
    }

    if (auto* tabbedSelector = dynamic_cast<TabbedSelector*>(component)) {
        var selectedIndex = PresetJson::property(command, "selectedIndex");
        String text = getString(command, "text");
        int tabIndex = -1;

        if (!selectedIndex.isVoid()) {
            tabIndex = int(selectedIndex);
        } else if (text.isNotEmpty()) {
            for (int i = 0; i < tabbedSelector->getNumTabs(); ++i) {
                if (tabbedSelector->getTabName(i).equalsIgnoreCase(text)) {
                    tabIndex = i;
                    break;
                }
            }
        } else {
            message = "TabbedSelector control requires selectedIndex or text";
            data = componentState(component, getString(command, "area"), getString(command, "target"));
            return false;
        }

        if (!isPositiveAndBelow(tabIndex, tabbedSelector->getNumTabs())) {
            message = "TabbedSelector tab was not found";
            data = componentState(component, getString(command, "area"), getString(command, "target"));
            return false;
        }

        tabbedSelector->selectTab(tabIndex);
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        message = "TabbedSelector control set";
        return true;
    }

    message = "Control target is not a supported Slider, Button, ComboBox, or TabbedSelector";
    data = componentState(component, getString(command, "area"), getString(command, "target"));
    return false;
}

bool CycleAutomation::resetMainPanelView(const var& command, String& message, var& data) {
    MainPanel& mainPanel = getObj(MainPanel);

    mainPanel.resetTabToWaveform();
    mainPanel.resized();
    drainMessageLoopIfRequested(command);

    data = componentState(&mainPanel, "AreaMain", {});
    message = "Main panel view reset";
    return true;
}

bool CycleAutomation::dismissTransientUi(const var& command, String& message, var& data) {
    auto& modMatrix = getObj(ModMatrixPanel);

    modMatrix.removeFromDesktop();
    modMatrix.setVisible(false);
    drainMessageLoopIfRequested(command);

    data = componentState(&modMatrix, "AreaModMatrix", {});
    message = "Transient UI dismissed";
    return true;
}

bool CycleAutomation::setCalloutCollapsed(const var& command, String& message, var& data) {
    Component* component = resolveComponent(command);

    if (component == nullptr) {
        message = "Callout target could not be resolved";
        return false;
    }

    auto* callout = dynamic_cast<RetractableCallout*>(component);

    if (callout == nullptr) {
        message = "Target is not a RetractableCallout";
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        return false;
    }

    callout->setAlwaysCollapsed(getBool(command, "collapsed", true));
    callout->resized();
    drainMessageLoopIfRequested(command);

    data = componentState(callout, getString(command, "area"), getString(command, "target"));
    message = "Callout collapsed state updated";
    return true;
}

bool CycleAutomation::pointer(const var& command, String& message, var& data) {
    Component* component = resolveComponent(command);

    if (component == nullptr) {
        message = "Pointer target could not be resolved";
        return false;
    }

    if (!component->isShowing()) {
        message = "Pointer target is not showing";
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        return false;
    }

    Rectangle<int> bounds = component->getLocalBounds();

    if (bounds.isEmpty()) {
        message = "Pointer target has empty bounds";
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        return false;
    }

    String eventType = getString(command, "event", getString(command, "pointerEvent", "click"));
    Point<float> position = getPointerPosition(command, *component, "x", "y");
    Point<float> downPosition = getPointerPosition(command, *component, "downX", "downY");
    bool waitedForIdle = drainMessageLoopIfRequested(command);

    if (eventType == "click") {
        component->mouseDown(makePointerEvent(*component, position, position, command, true, false, 1));
        component->mouseUp(makePointerEvent(*component, position, position, command, false, false, 1));
    } else if (eventType == "doubleClick") {
        component->mouseDoubleClick(makePointerEvent(*component, position, position, command, true, false, 2));
    } else if (eventType == "down") {
        component->mouseDown(makePointerEvent(*component, position, position, command, true, false, 1));
    } else if (eventType == "up") {
        component->mouseUp(makePointerEvent(*component, position, downPosition, command, false, false, 1));
    } else if (eventType == "drag") {
        component->mouseDrag(makePointerEvent(*component, position, downPosition, command, true, true, 1));
    } else if (eventType == "move") {
        component->mouseMove(makePointerEvent(*component, position, position, command, false, false, 0));
    } else if (eventType == "enter") {
        component->mouseEnter(makePointerEvent(*component, position, position, command, false, false, 0));
    } else if (eventType == "exit") {
        component->mouseExit(makePointerEvent(*component, position, position, command, false, false, 0));
    } else if (eventType == "wheel") {
        MouseWheelDetails wheel {
            float(getDouble(command, "deltaX")),
            float(getDouble(command, "deltaY")),
            getBool(command, "reversed"),
            getBool(command, "smooth", true),
            getBool(command, "inertial"),
        };

        component->mouseWheelMove(makePointerEvent(*component, position, position, command, false, false, 0), wheel);
    } else {
        message = "Unknown pointer event: " + eventType;
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        return false;
    }

    auto json = PresetJson::object();
    json->setProperty("event", eventType);
    json->setProperty("area", getString(command, "area"));
    json->setProperty("target", getString(command, "target"));
    json->setProperty("x", position.x);
    json->setProperty("y", position.y);
    json->setProperty("waitedForIdle", waitedForIdle);
    json->setProperty("localBounds", rectangleState(bounds));
    json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));
    data = PresetJson::toVar(json);

    message = "Pointer event executed";
    return true;
}

bool CycleAutomation::assertTarget(const var& command, String& message, var& data) {
    String area = getString(command, "area");
    String target = getString(command, "target");
    String path = getString(command, "path");
    String op = assertionOperator(command);
    double tolerance = getDouble(command, "tolerance", 0.000001);
    drainMessageLoopIfRequested(command);
    Component* component = resolveComponent(command);

    if (component == nullptr) {
        message = "Assertion target could not be resolved";
        return false;
    }

    if (path.isEmpty()) {
        message = "assertTarget requires path";
        return false;
    }

    if (op.isEmpty()) {
        message = "assertTarget requires an assertion operator";
        return false;
    }

    var state = componentState(component, area, target);
    var actual;
    bool pathFound = getPathValue(state, path, actual);
    var expected = PresetJson::property(command, op);

    auto json = PresetJson::object();
    json->setProperty("area", area);
    json->setProperty("target", target);
    json->setProperty("path", path);
    json->setProperty("operator", op);
    json->setProperty("pathFound", pathFound);

    if (pathFound) {
        json->setProperty("actual", actual);
    }

    if (op != "exists") {
        json->setProperty("expected", expected);
    }

    data = PresetJson::toVar(json);

    if (!pathFound) {
        message = "Assertion path not found: " + path;
        return false;
    }

    if (!compareVars(actual, op, expected, tolerance)) {
        message = "Assertion failed for path " + path + ": actual=" + actual.toString();

        if (op != "exists") {
            message += " expected=" + expected.toString();
        }

        return false;
    }

    message = "Assertion passed";
    return true;
}

bool CycleAutomation::assertState(const var& command, String& message, var& data) {
    String path = getString(command, "path");
    String op = assertionOperator(command);
    double tolerance = getDouble(command, "tolerance", 0.000001);
    drainMessageLoopIfRequested(command);

    if (path.isEmpty()) {
        message = "assertState requires path";
        return false;
    }

    if (op.isEmpty()) {
        message = "assertState requires an assertion operator";
        return false;
    }

    var state = snapshotState();
    var actual;
    bool pathFound = getPathValue(state, path, actual);
    var expected = PresetJson::property(command, op);

    auto json = PresetJson::object();
    json->setProperty("path", path);
    json->setProperty("operator", op);
    json->setProperty("pathFound", pathFound);

    if (pathFound) {
        json->setProperty("actual", actual);
    }

    if (op != "exists") {
        json->setProperty("expected", expected);
    }

    data = PresetJson::toVar(json);

    if (!pathFound) {
        message = "Assertion path not found: " + path;
        return false;
    }

    if (!compareVars(actual, op, expected, tolerance)) {
        message = "Assertion failed for path " + path + ": actual=" + actual.toString();

        if (op != "exists") {
            message += " expected=" + expected.toString();
        }

        return false;
    }

    message = "Assertion passed";
    return true;
}

bool CycleAutomation::listAssertionPaths(const var& command, String& message, var& data) {
    String scope = getString(command, "scope", "snapshot");
    int maxDepth = jlimit(1, 16, int(getDouble(command, "maxDepth", 8.0)));
    var source;

    if (scope == "target") {
        Component* component = resolveComponent(command);

        if (component == nullptr) {
            message = "Assertion path target could not be resolved";
            return false;
        }

        source = componentState(component, getString(command, "area"), getString(command, "target"));
    } else if (scope == "snapshot") {
        source = snapshotState();
    } else {
        message = "Unknown assertion path scope: " + scope;
        return false;
    }

    Array<var> paths;
    flattenAssertionPaths(source, {}, paths, 0, maxDepth);

    auto json = PresetJson::object();
    json->setProperty("scope", scope);
    json->setProperty("area", getString(command, "area"));
    json->setProperty("target", getString(command, "target"));
    json->setProperty("maxDepth", maxDepth);
    json->setProperty("count", paths.size());
    json->setProperty("paths", var(paths));
    data = PresetJson::toVar(json);

    message = "Listed " + String(paths.size()) + " assertion paths";
    return true;
}

bool CycleAutomation::listMeshTargets(const var& command, String& message, var& data) {
    bool includeLayers = getBool(command, "includeLayers", true);
    MeshLibrary& meshLibrary = getObj(MeshLibrary);
    Array<var> groups;

    {
        ScopedLock sl(meshLibrary.getLock());

        for (int groupId = 0; groupId < meshLibrary.getNumGroups(); ++groupId) {
            String groupName = meshLayerGroupName(groupId);

            if (!meshGroupMatches(command, groupId, groupName)) {
                continue;
            }

            MeshLibrary::LayerGroup& group = meshLibrary.getLayerGroup(groupId);
            auto groupJson = PresetJson::object();

            groupJson->setProperty("id", groupId);
            groupJson->setProperty("name", groupName);
            groupJson->setProperty("meshType", group.meshType);
            groupJson->setProperty("meshTypeName", meshTypeName(group.meshType));
            groupJson->setProperty("currentLayer", group.current);
            groupJson->setProperty("layerCount", group.size());
            groupJson->setProperty("selectedCount", int(group.selected.size()));
            groupJson->setProperty("hasPreviewMesh", group.previewMesh != nullptr);
            groupJson->setProperty("currentMesh", meshSummary(group.getCurrentMesh()));
            groupJson->setProperty("effectiveMesh", meshSummary(meshLibrary.getEffectiveMesh(groupId)));

            if (isPositiveAndBelow(group.current, group.size())) {
                MeshLibrary::Layer& currentLayer = group.layers[group.current];

                if (currentLayer.props != nullptr) {
                    groupJson->setProperty("currentProperties", currentLayer.props->writeJSON());
                }
            }

            if (includeLayers) {
                Array<var> layers;

                for (int layerIndex = 0; layerIndex < group.size(); ++layerIndex) {
                    layers.add(layerSummary(groupId, groupName, layerIndex, group.current, group.layers[layerIndex]));
                }

                groupJson->setProperty("layers", var(layers));
            }

            groups.add(PresetJson::toVar(groupJson));
        }
    }

    auto json = PresetJson::object();
    json->setProperty("count", groups.size());
    json->setProperty("groups", var(groups));
    json->setProperty("includeLayers", includeLayers);
    data = PresetJson::toVar(json);

    if (groups.isEmpty()) {
        message = "No mesh target groups matched";
        return false;
    }

    message = "Listed " + String(groups.size()) + " mesh target groups";
    return true;
}

bool CycleAutomation::exportMeshState(const var& command, String& message, var& data) {
    MeshLibrary& meshLibrary = getObj(MeshLibrary);
    String path = getString(command, "path");
    int groupId = CommonEnums::Null;
    int layerIndex = CommonEnums::Null;

    {
        ScopedLock sl(meshLibrary.getLock());

        groupId = meshGroupIdForCommand(command, meshLibrary);

        if (groupId == CommonEnums::Null) {
            message = "exportMeshState requires a valid group or groupId";
            return false;
        }

        MeshLibrary::LayerGroup& group = meshLibrary.getLayerGroup(groupId);
        layerIndex = meshLayerIndexForCommand(command, group);

        if (layerIndex == CommonEnums::Null) {
            message = "exportMeshState requires a valid layer or layerIndex";
            return false;
        }

        data = exportedMeshLayerState(groupId,
                                      meshLayerGroupName(groupId),
                                      group.meshType,
                                      layerIndex,
                                      group.current,
                                      group.layers[layerIndex]);
    }

    if (path.isNotEmpty()) {
        File(path).replaceWithText(JSON::toString(data, true));
    }

    message = path.isNotEmpty()
        ? "Mesh state exported: " + path
        : "Mesh state exported";
    return true;
}

bool CycleAutomation::mutateMeshVertex(const var& command, String& message, var& data) {
    String type = getString(command, "command", getString(command, "type"));
    String area = meshAreaForCommand(command);
    int vertexIndex = int(getDouble(command, "vertexIndex", getDouble(command, "index", -1.0)));
    float x = float(getDouble(command, "x", getDouble(command, "phase", 0.5)));
    float y = float(getDouble(command, "y", getDouble(command, "amp", 0.5)));
    float dx = float(getDouble(command, "dx", getDouble(command, "deltaX", 0.0)));
    float dy = float(getDouble(command, "dy", getDouble(command, "deltaY", 0.0)));
    float curve = float(getDouble(command, "curve", getDouble(command, "value", 0.0)));
    bool updates = getBool(command, "updates", true);
    String mode = getString(command, "mode", getString(command, "driver", "gesture"));
    String actionType;
    bool ok = false;

    if (area.isEmpty()) {
        message = "Mesh mutation requires a supported area or group";
        return false;
    }

    if (type == "selectVertex") {
        actionType = "SelectPoint";
    } else if (type == "addVertex") {
        actionType = "AddPoint";
    } else if (type == "moveVertex") {
        actionType = "MovePoint";
        x = dx;
        y = dy;
    } else if (type == "deleteVertex") {
        actionType = "DeletePoint";
    } else {
        message = "Unknown mesh mutation command: " + type;
        return false;
    }

    if (type != "addVertex" && vertexIndex < 0) {
        message = type + " requires vertexIndex or index";
        return false;
    }

    if (mode == "patch") {
        if (type == "moveVertex") {
            ok = moveMeshVertexDirectly(getObj(MeshLibrary), command, area, vertexIndex, dx, dy);
        } else if (type == "deleteVertex") {
            ok = deleteMeshVertexDirectly(getObj(MeshLibrary), command, area, vertexIndex);
        } else {
            ok = runMeshTourAction(getObj(CycleTour), actionType, area, vertexIndex, x, y, curve, updates);
        }
    } else if (mode == "tour") {
        ok = runMeshTourAction(getObj(CycleTour), actionType, area, vertexIndex, x, y, curve, updates);
    } else if (mode == "gesture") {
        ok = runMeshGesture(getObj(CycleTour),
                            getObj(KeyboardInputHandler),
                            getObj(MeshLibrary),
                            getObj(Settings),
                            command,
                            type,
                            area,
                            vertexIndex,
                            x,
                            y,
                            dx,
                            dy,
                            message);
    } else {
        message = "Unknown mesh mutation mode: " + mode;
        return false;
    }

    drainMessageLoopIfRequested(command);

    auto json = PresetJson::object();
    json->setProperty("command", type);
    json->setProperty("actionType", actionType);
    json->setProperty("mode", mode);
    json->setProperty("area", area);
    json->setProperty("vertexIndex", vertexIndex);
    json->setProperty("x", x);
    json->setProperty("y", y);
    json->setProperty("curve", curve);
    json->setProperty("updates", updates);
    json->setProperty("meshTargets", listMeshTargets(command, message, data) ? data : var());
    data = PresetJson::toVar(json);

    if (!ok) {
        message = "Mesh mutation did not execute: " + type;
        return false;
    }

    message = "Mesh mutation executed: " + type;
    return true;
}

bool CycleAutomation::meshSelectionGesture(const var& command, String& message, var& data) {
    String area = meshAreaForCommand(command);
    String gesture = getString(command, "gesture", getString(command, "action", "boxSelect"));

    if (area.isEmpty()) {
        message = "meshSelectionGesture requires a supported area or group";
        return false;
    }

    MeshLibrary& meshLibrary = getObj(MeshLibrary);
    Settings& settings = getObj(Settings);
    Interactor* interactor = resolveMeshGestureInteractor(getObj(CycleTour), meshLibrary, command, area, message);

    if (interactor == nullptr) {
        return false;
    }

    const bool selectWithRight = settings.getGlobalSetting(AppSettings::SelectWithRight) == 1;
    const bool selectButtonIsRight = selectWithRight;
    const bool boxButtonIsRight = !selectWithRight;
    int previousTool = settings.getGlobalSetting(AppSettings::Tool);
    settings.getGlobalSetting(AppSettings::Tool) = Tools::Selector;
    getObj(KeyboardInputHandler).setFocusedInteractor(interactor, true);

    if (gesture == "clear" || gesture == "deselectAll") {
        interactor->deselectAll(true);
    } else if (gesture == "boxSelect") {
        Point<float> from = meshPointToLocal(*interactor,
                                             float(getDouble(command, "fromX", getDouble(command, "x1", 0.0))),
                                             float(getDouble(command, "fromY", getDouble(command, "y1", 0.0))));
        Point<float> to = meshPointToLocal(*interactor,
                                           float(getDouble(command, "toX", getDouble(command, "x2", 1.0))),
                                           float(getDouble(command, "toY", getDouble(command, "y2", 1.0))));

        dispatchMeshDrag(*interactor, from, to, boxButtonIsRight, true, getBool(command, "command"));
    } else if (gesture == "dragHandle" || gesture == "moveSelection") {
        String handleName = gesture == "moveSelection" ? "move" : getString(command, "handle", "move");
        int handle = selectionHandleForName(handleName);

        if (handle == CommonEnums::Null || interactor->selectionCorners.empty()) {
            settings.getGlobalSetting(AppSettings::Tool) = previousTool;
            message = "meshSelectionGesture requires an active selection handle";
            return false;
        }

        Point<float> from = selectionHandlePoint(*interactor, handle);
        Point<float> to = from + Point<float>(float(getDouble(command, "dx", 0.0)),
                                             float(getDouble(command, "dy", 0.0)));
        var toX = PresetJson::property(command, "toX");
        var toY = PresetJson::property(command, "toY");

        if (!toX.isVoid() && !toY.isVoid()) {
            to = meshPointToLocal(*interactor, float(double(toX)), float(double(toY)));
        }

        dispatchMeshDrag(*interactor, from, to, selectButtonIsRight, false, getBool(command, "command"));
    } else {
        settings.getGlobalSetting(AppSettings::Tool) = previousTool;
        message = "Unknown mesh selection gesture: " + gesture;
        return false;
    }

    settings.getGlobalSetting(AppSettings::Tool) = previousTool;
    drainMessageLoopIfRequested(command);

    auto json = PresetJson::object();
    json->setProperty("gesture", gesture);
    json->setProperty("area", area);
    json->setProperty("selection", meshSelectionState(*interactor));
    json->setProperty("meshTargets", listMeshTargets(command, message, data) ? data : var());
    data = PresetJson::toVar(json);

    message = "Mesh selection gesture executed: " + gesture;
    return true;
}

bool CycleAutomation::waitForIdle(const var& command, String& message, var& data) {
    int delayMs = getIdleDelayMs(command);
    bool dispatched = drainMessageLoop(delayMs);

    auto json = PresetJson::object();
    json->setProperty("delayMs", delayMs);
    json->setProperty("messageLoopDispatched", dispatched);
    data = PresetJson::toVar(json);

    message = "Idle wait completed";
    return true;
}

var CycleAutomation::snapshotState() {
    auto snapshot = PresetJson::object();
    auto panels = PresetJson::object();

    snapshot->setProperty("document", getObj(Document).getDocumentName());
    snapshot->setProperty("automationRan", hasRun);
    snapshot->setProperty("drawWave", getSetting(DrawWave) != 0);
    snapshot->setProperty("waveLoaded", getSetting(WaveLoaded) != 0);
    snapshot->setProperty("wavePitchLayerCount",
                          getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupWavePitch).size());

    if (auto* controls = getObj(CycleTour).getAutomationInspectable("AreaGenControls", {})) {
        var state = controls->exportAutomationState();
        panels->setProperty("generalControls", state);
        setIfPathExists(*snapshot, "currentTool", state, "tools.name");
        setIfPathExists(*snapshot, "currentToolId", state, "tools.current");
        setIfPathExists(*snapshot, "viewStage", state, "viewStage");
        setIfPathExists(*snapshot, "viewStageName", state, "viewStageName");
    }

    if (auto* morphPanel = getObj(CycleTour).getAutomationInspectable("AreaMorphPanel", {})) {
        var state = morphPanel->exportAutomationState();
        panels->setProperty("morphPanel", state);
        setIfPathExists(*snapshot, "morphPosition", state, "sliders");
        setIfPathExists(*snapshot, "morphLinking", state, "linking");
        setIfPathExists(*snapshot, "morphRangeEnabled", state, "rangeEnabled");
    }

    if (auto* waveform3D = getObj(CycleTour).getAutomationInspectable("AreaWfrmWaveform3D", {})) {
        var state = waveform3D->exportAutomationState();
        panels->setProperty("waveform3D", state);
        setIfPathExists(*snapshot, "timeLayer", state, "layer");
    }

    snapshot->setProperty("panels", PresetJson::toVar(panels));

    return PresetJson::toVar(snapshot);
}

Component* CycleAutomation::resolveComponent(const var& command) {
    String area = getString(command, "area");
    String target = getString(command, "target");

    if (area.isNotEmpty()) {
        return getObj(CycleTour).getComponent(area, target);
    }

    return &getObj(MainPanel);
}

var CycleAutomation::componentState(Component* component, const String& area, const String& target) const {
    auto json = PresetJson::object();
    json->setProperty("area", area);
    json->setProperty("target", target);
    json->setProperty("resolved", component != nullptr);

    if (component != nullptr) {
        json->setProperty("name", component->getName());
        json->setProperty("class", String(typeid(*component).name()));
        json->setProperty("visible", component->isVisible());
        json->setProperty("showing", component->isShowing());
        json->setProperty("enabled", component->isEnabled());
        json->setProperty("localBounds", rectangleState(component->getLocalBounds()));
        json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));

    if (auto* openGL = dynamic_cast<OpenGLBase*>(component)) {
        json->setProperty("openGL", openGLDiagnosticsState(*openGL, false, "componentState"));
    }

    if (auto* slider = dynamic_cast<Slider*>(component)) {
        json->setProperty("controlType", "slider");
        json->setProperty("value", slider->getValue());
        json->setProperty("minimum", slider->getMinimum());
        json->setProperty("maximum", slider->getMaximum());
        json->setProperty("interval", slider->getInterval());
        json->setProperty("text", slider->getTextFromValue(slider->getValue()));
    } else if (auto* button = dynamic_cast<Button*>(component)) {
        json->setProperty("controlType", "button");
        json->setProperty("toggleState", button->getToggleState());
        json->setProperty("buttonText", button->getButtonText());
    } else if (auto* comboBox = dynamic_cast<ComboBox*>(component)) {
        Array<var> items;

        for (int i = 0; i < comboBox->getNumItems(); ++i) {
            auto item = PresetJson::object();
            item->setProperty("index", i);
            item->setProperty("id", comboBox->getItemId(i));
            item->setProperty("text", comboBox->getItemText(i));
            items.add(PresetJson::toVar(item));
        }

        json->setProperty("controlType", "comboBox");
        json->setProperty("selectedId", comboBox->getSelectedId());
        json->setProperty("selectedIndex", comboBox->getSelectedItemIndex());
        json->setProperty("text", comboBox->getText());
        json->setProperty("items", var(items));
    } else if (auto* tabbedSelector = dynamic_cast<TabbedSelector*>(component)) {
        Array<var> tabs;

        for (int i = 0; i < tabbedSelector->getNumTabs(); ++i) {
            auto item = PresetJson::object();
            item->setProperty("index", i);
            item->setProperty("text", tabbedSelector->getTabName(i));
            item->setProperty("selected", tabbedSelector->getSelectedId() == i);
            item->setProperty("localBounds", rectangleState(tabbedSelector->getTabBounds(i)));
            tabs.add(PresetJson::toVar(item));
        }

        json->setProperty("controlType", "tabbedSelector");
        json->setProperty("selectedIndex", tabbedSelector->getSelectedId());
        json->setProperty("tabCount", tabbedSelector->getNumTabs());
        json->setProperty("tabs", var(tabs));
    } else if (auto* selector = dynamic_cast<SelectorPanel*>(component)) {
        json->setProperty("controlType", "selectorPanel");
        json->setProperty("currentIndex", selector->getCurrentIndexExternal());
        json->setProperty("displayIndex", selector->getCurrentIndexExternal() + 1);
        json->setProperty("itemCount", selector->getSize());
    } else if (auto* hoverSelector = dynamic_cast<HoverSelector*>(component)) {
        json->setProperty("controlType", "hoverSelector");
        json->setProperty("menuActive", hoverSelector->menuActive);
        json->setProperty("horizontal", hoverSelector->horizontal);
    } else if (auto* midiKeyboard = dynamic_cast<MidiKeyboard*>(component)) {
        json->setProperty("controlType", "midiKeyboard");
        json->setProperty("auditionKey", midiKeyboard->getAuditionKey());
        json->setProperty("auditionKeyName", MidiKeyboard::getText(midiKeyboard->getAuditionKey()));
    } else if (auto* playbackPanel = dynamic_cast<PlaybackPanel*>(component)) {
        json->setProperty("controlType", "playbackPanel");
        json->setProperty("progress", playbackPanel->getProgress());
        json->setProperty("playing", playbackPanel->isPlaying());
        json->setProperty("envelopePosition", playbackPanel->getEnvelopePos());
    } else if (dynamic_cast<BannerPanel*>(component) != nullptr) {
        json->setProperty("controlType", "bannerPanel");
    } else if (dynamic_cast<DerivativePanel*>(component) != nullptr) {
        json->setProperty("controlType", "derivativePanel");
    } else if (auto* dragger = dynamic_cast<Dragger*>(component)) {
        json->setProperty("controlType", "dragger");

        if (PanelPair* pair = dragger->getPair()) {
            json->setProperty("portion", pair->getPortion());
            json->setProperty("orientation", pair->sideBySide ? "vertical" : "horizontal");
            json->setProperty("pairBounds", rectangleState(pair->getBounds()));
        }
    } else if (auto* pullout = dynamic_cast<PulloutComponent*>(component)) {
        json->setProperty("controlType", "pulloutComponent");
        json->setProperty("orientation", pullout->isHorizontal() ? "horizontal" : "vertical");
        json->setProperty("popupVisible", pullout->isPopupVisible());
        json->setProperty("popupButtonCount", pullout->getPopupButtonCount());
        json->setProperty("popupBounds", rectangleState(pullout->getPopupBounds()));
    } else if (auto* callout = dynamic_cast<RetractableCallout*>(component)) {
        json->setProperty("controlType", "retractableCallout");
        json->setProperty("currentlyCollapsed", callout->isCurrentlyCollapsed());
        json->setProperty("sizeCollapsed", callout->isCollapsed());
        json->setProperty("alwaysCollapsed", callout->isAlwaysCollapsed());
        json->setProperty("expandedSize", callout->getExpandedSize());
        json->setProperty("collapsedSize", callout->getCollapsedSize());
    } else {
        json->setProperty("controlType", "component");
    }

        if (auto* inspectable = const_cast<CycleTour&>(getObj(CycleTour)).getAutomationInspectable(area, target)) {
            json->setProperty("automationState", inspectable->exportAutomationState());
        }
    } else {
        json->setProperty("error", "Target could not be resolved");
    }

    return PresetJson::toVar(json);
}

var CycleAutomation::registeredTargetsForRoot(Component* root, const String& requestedArea) const {
    Array<var> targets;
    Rectangle<int> rootBounds = root != nullptr ? root->getScreenBounds() : Rectangle<int>();
    CycleTour& tour = const_cast<CycleTour&>(getObj(CycleTour));

    if (root == nullptr) {
        return var(targets);
    }

    for (auto* area : kTourGuideAreas) {
        if (requestedArea.isNotEmpty() && requestedArea != area) {
            continue;
        }

        for (auto* target : kInspectableTargets) {
            Component* component = tour.getComponent(area, target);

            if (component == nullptr) {
                continue;
            }

            Rectangle<int> targetBounds = component->getScreenBounds();

            if (component == root || root->isParentOf(component) || rootBounds.intersects(targetBounds)) {
                targets.add(componentState(component, area, target));
            }
        }
    }

    return var(targets);
}

var CycleAutomation::componentTreeState(Component* component,
                                        const String& path,
                                        int depth,
                                        int maxDepth,
                                        int maxNodes,
                                        int& nodeCount,
                                        bool includeInvisible) const {
    auto json = PresetJson::object();

    if (component == nullptr || nodeCount >= maxNodes) {
        json->setProperty("truncated", true);
        return PresetJson::toVar(json);
    }

    ++nodeCount;

    json->setProperty("path", path);
    json->setProperty("depth", depth);
    json->setProperty("name", component->getName());
    json->setProperty("class", String(typeid(*component).name()));
    json->setProperty("role", componentRole(component));
    json->setProperty("visible", component->isVisible());
    json->setProperty("showing", component->isShowing());
    json->setProperty("enabled", component->isEnabled());
    json->setProperty("localBounds", rectangleState(component->getLocalBounds()));
    json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));
    annotateTourTarget(*json, component);

    if (auto* slider = dynamic_cast<Slider*>(component)) {
        json->setProperty("value", slider->getValue());
        json->setProperty("minimum", slider->getMinimum());
        json->setProperty("maximum", slider->getMaximum());
        json->setProperty("text", slider->getTextFromValue(slider->getValue()));
    } else if (auto* button = dynamic_cast<Button*>(component)) {
        json->setProperty("toggleState", button->getToggleState());
        json->setProperty("buttonText", button->getButtonText());
    } else if (auto* tabbedSelector = dynamic_cast<TabbedSelector*>(component)) {
        json->setProperty("selectedIndex", tabbedSelector->getSelectedId());
        json->setProperty("tabCount", tabbedSelector->getNumTabs());
    }

    if (depth >= maxDepth || nodeCount >= maxNodes) {
        json->setProperty("childrenTruncated", component->getNumChildComponents() > 0);
        return PresetJson::toVar(json);
    }

    Array<var> children;

    for (int i = 0; i < component->getNumChildComponents() && nodeCount < maxNodes; ++i) {
        Component* child = component->getChildComponent(i);

        if (child == nullptr) {
            continue;
        }

        if (!includeInvisible && !child->isVisible()) {
            continue;
        }

        children.add(componentTreeState(child, path + "/" + String(i), depth + 1, maxDepth, maxNodes, nodeCount, includeInvisible));
    }

    json->setProperty("children", var(children));
    return PresetJson::toVar(json);
}

void CycleAutomation::annotateTourTarget(DynamicObject& json, Component* component) const {
    CycleTour& tour = const_cast<CycleTour&>(getObj(CycleTour));

    for (auto* area : kInspectableAreas) {
        if (tour.getComponent(area, {}) == component) {
            json.setProperty("area", area);
            break;
        }
    }

    for (auto* area : kTourGuideAreas) {
        for (auto* target : kInspectableTargets) {
            if (tour.getComponent(area, target) == component) {
                json.setProperty("area", area);
                json.setProperty("target", target);
                return;
            }
        }
    }
}
