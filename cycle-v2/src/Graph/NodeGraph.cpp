#include "Graph/NodeGraph.h"

#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/NodeParameterMap.h"

#include "Graph/NodeDefinition.h"

#include "Nodes/Guide/GuideHeatmapAsset.h"

#include <algorithm>
#include <unordered_set>

namespace CycleV2 {

namespace {

template<typename Container, typename Predicate>
void eraseIf(Container& container, Predicate predicate) {
    container.erase(
            std::remove_if(
                    container.begin(),
                    container.end(),
                    predicate),
            container.end());
}

}

void NodeGraph::addNode(Node nodeToAdd) {
    if (findNode(nodeToAdd.id) != nullptr) {
        return;
    }
    nodes.push_back(std::move(nodeToAdd));
    nodeIndex[nodes.back().id] = nodes.size() - 1;
    rebuildParameterIndex(nodes.back().id);
    ++revision;
}

void NodeGraph::addEdge(Edge edgeToAdd) {
    const auto duplicate = std::find_if(edges.begin(), edges.end(), [&](const Edge& edge) {
        return edge.sourceNodeId == edgeToAdd.sourceNodeId
                && edge.sourcePortId == edgeToAdd.sourcePortId
                && edge.destNodeId == edgeToAdd.destNodeId
                && edge.destPortId == edgeToAdd.destPortId
                && edge.connectionKind == edgeToAdd.connectionKind
                && edge.attachmentType == edgeToAdd.attachmentType;
    });
    if (duplicate != edges.end()) {
        return;
    }

    edges.push_back(std::move(edgeToAdd));
    ++revision;
}

bool NodeGraph::addGuideCurve(GuideCurveResource resource) {
    if (resource.id.isEmpty() || findGuideCurve(resource.id) != nullptr) {
        return false;
    }

    guideCurves.push_back(std::move(resource));
    guideResourceIndex[guideCurves.back().id] = guideCurves.size() - 1;
    ++revision;
    return true;
}

bool NodeGraph::removeGuideCurve(const String& guideId) {
    const size_t previousCount = guideCurves.size();
    eraseIf(guideCurves, [&](const GuideCurveResource& resource) {
        return resource.id == guideId;
    });
    if (guideCurves.size() == previousCount) {
        return false;
    }

    eraseIf(guideAssignments, [&](const GuideCurveAssignment& assignment) {
        return assignment.guideId == guideId;
    });
    rebuildGuideResourceIndex();
    rebuildGuideAssignmentIndexes();
    removeUnreferencedGuideHeatmaps();
    ++revision;
    return true;
}

const GuideCurveResource* NodeGraph::findGuideCurve(const String& guideId) const {
    const auto found = guideResourceIndex.find(guideId);
    const GuideCurveResource* local = found != guideResourceIndex.end()
                    && found->second < guideCurves.size()
            ? &guideCurves[found->second]
            : nullptr;
    return local != nullptr || overlayBase == nullptr
            ? local
            : overlayBase->findGuideCurve(guideId);
}

GuideCurveResource* NodeGraph::findGuideCurveForEditing(const String& guideId) {
    const auto found = guideResourceIndex.find(guideId);
    if (found != guideResourceIndex.end() && found->second < guideCurves.size()) {
        return &guideCurves[found->second];
    }
    const GuideCurveResource* baseGuide = overlayBase != nullptr
            ? overlayBase->findGuideCurve(guideId)
            : nullptr;
    if (baseGuide == nullptr) {
        return nullptr;
    }

    guideCurves.push_back(*baseGuide);
    guideResourceIndex[guideId] = guideCurves.size() - 1;
    return &guideCurves.back();
}

bool NodeGraph::replaceGuideCurve(GuideCurveResource resource) {
    GuideCurveResource* existing = findGuideCurveForEditing(resource.id);
    if (existing == nullptr) {
        return false;
    }

    *existing = std::move(resource);
    ++revision;
    return true;
}

bool NodeGraph::addGuideHeatmap(GuideHeatmapAssetPtr asset) {
    if (asset == nullptr || asset->id().isEmpty()) {
        return false;
    }
    if (findGuideHeatmap(asset->id()) != nullptr) {
        return true;
    }
    guideHeatmaps.push_back(std::move(asset));
    guideHeatmapIndex[guideHeatmaps.back()->id()] = guideHeatmaps.size() - 1;
    ++revision;
    return true;
}

const GuideHeatmapAsset* NodeGraph::findGuideHeatmap(const String& assetId) const {
    const auto found = guideHeatmapIndex.find(assetId);
    const GuideHeatmapAsset* local = found != guideHeatmapIndex.end()
                    && found->second < guideHeatmaps.size()
            ? guideHeatmaps[found->second].get()
            : nullptr;
    return local != nullptr || overlayBase == nullptr
            ? local
            : overlayBase->findGuideHeatmap(assetId);
}

GuideHeatmapAssetPtr NodeGraph::guideHeatmapAsset(const String& assetId) const {
    const auto found = guideHeatmapIndex.find(assetId);
    const GuideHeatmapAssetPtr local = found != guideHeatmapIndex.end()
            ? guideHeatmaps[found->second]
            : GuideHeatmapAssetPtr();
    return local != nullptr || overlayBase == nullptr
            ? local
            : overlayBase->guideHeatmapAsset(assetId);
}

void NodeGraph::removeUnreferencedGuideHeatmaps() {
    std::unordered_set<String, StringHash> referenced;
    for (const auto& guide : guideCurves) {
        if (guide.heatmapAssetId.isNotEmpty()) {
            referenced.insert(guide.heatmapAssetId);
        }
    }
    eraseIf(guideHeatmaps, [&](const GuideHeatmapAssetPtr& asset) {
        return asset == nullptr || referenced.find(asset->id()) == referenced.end();
    });
    guideHeatmapIndex.clear();
    for (size_t index = 0; index < guideHeatmaps.size(); ++index) {
        guideHeatmapIndex[guideHeatmaps[index]->id()] = index;
    }
}

bool NodeGraph::moveGuideCurve(const String& guideId, int shelfOrder) {
    const auto source = std::find_if(guideCurves.begin(), guideCurves.end(), [&](const auto& guide) {
        return guide.id == guideId;
    });
    if (source == guideCurves.end()) {
        return false;
    }

    const int sourceIndex = (int) std::distance(guideCurves.begin(), source);
    const int destinationIndex = jlimit(0, (int) guideCurves.size() - 1, shelfOrder);
    if (sourceIndex == destinationIndex) {
        return false;
    }

    if (sourceIndex < destinationIndex) {
        std::rotate(source, source + 1, guideCurves.begin() + destinationIndex + 1);
    } else {
        std::rotate(guideCurves.begin() + destinationIndex, source, source + 1);
    }
    for (int index = 0; index < (int) guideCurves.size(); ++index) {
        guideCurves[(size_t) index].shelfOrder = index;
    }
    rebuildGuideResourceIndex();
    ++revision;
    return true;
}

bool NodeGraph::assignGuideCurve(GuideCurveAssignment assignment) {
    if (findGuideCurve(assignment.guideId) == nullptr
            || findNode(assignment.targetNodeId) == nullptr
            || assignment.target.cubeIndex < 0) {
        return false;
    }

    const auto target = guideAssignmentTargetIndex.find({
            assignment.targetNodeId,
            assignment.target
    });
    if (target != guideAssignmentTargetIndex.end()) {
        GuideCurveAssignment& existing = guideAssignments[target->second];
        if (existing.guideId == assignment.guideId) {
            return false;
        }
        existing = std::move(assignment);
        rebuildGuideAssignmentIndexes();
        ++revision;
        return true;
    }

    guideAssignments.push_back(std::move(assignment));
    rebuildGuideAssignmentIndexes();
    ++revision;
    return true;
}

bool NodeGraph::addAudioResource(AudioSampleResource resource) {
    if (resource.id.isEmpty() || findAudioResource(resource.id) != nullptr) {
        return false;
    }

    audioResources.push_back(std::move(resource));
    ++revision;
    return true;
}

bool NodeGraph::removeAudioResource(const String& resourceId) {
    if (audioResourceUsageCount(resourceId) != 0) {
        return false;
    }

    const size_t previousCount = audioResources.size();
    eraseIf(audioResources, [&](const AudioSampleResource& resource) {
        return resource.id == resourceId;
    });
    if (audioResources.size() == previousCount) {
        return false;
    }

    ++revision;
    return true;
}

const AudioSampleResource* NodeGraph::findAudioResource(const String& resourceId) const {
    const auto found = std::find_if(audioResources.begin(), audioResources.end(), [&](const auto& resource) {
        return resource.id == resourceId;
    });
    const AudioSampleResource* local = found != audioResources.end() ? &*found : nullptr;
    return local != nullptr || overlayBase == nullptr
            ? local
            : overlayBase->findAudioResource(resourceId);
}

bool NodeGraph::bindAudioResource(NodeAudioResourceBinding binding) {
    if (binding.nodeId.isEmpty() || binding.resourceId.isEmpty() || binding.mode.isEmpty()
            || findNode(binding.nodeId) == nullptr
            || findAudioResource(binding.resourceId) == nullptr) {
        return false;
    }

    const auto found = std::find_if(
            audioResourceBindings.begin(),
            audioResourceBindings.end(),
            [&](const auto& existing) { return existing.nodeId == binding.nodeId; });
    if (found != audioResourceBindings.end()) {
        if (found->resourceId == binding.resourceId && found->mode == binding.mode) {
            return false;
        }
        *found = std::move(binding);
    } else {
        audioResourceBindings.push_back(std::move(binding));
    }
    ++revision;
    return true;
}

bool NodeGraph::unbindAudioResource(const String& nodeId) {
    const size_t previousCount = audioResourceBindings.size();
    eraseIf(audioResourceBindings, [&](const auto& binding) {
        return binding.nodeId == nodeId;
    });
    if (audioResourceBindings.size() == previousCount) {
        return false;
    }

    ++revision;
    return true;
}

const NodeAudioResourceBinding* NodeGraph::findAudioResourceBinding(const String& nodeId) const {
    const auto found = std::find_if(
            audioResourceBindings.begin(),
            audioResourceBindings.end(),
            [&](const auto& binding) { return binding.nodeId == nodeId; });
    const NodeAudioResourceBinding* local = found != audioResourceBindings.end()
            ? &*found
            : nullptr;
    return local != nullptr || overlayBase == nullptr
            ? local
            : overlayBase->findAudioResourceBinding(nodeId);
}

int NodeGraph::audioResourceUsageCount(const String& resourceId) const {
    if (overlayBase != nullptr && audioResourceBindings.empty()) {
        return overlayBase->audioResourceUsageCount(resourceId);
    }
    return (int) std::count_if(
            audioResourceBindings.begin(),
            audioResourceBindings.end(),
            [&](const auto& binding) { return binding.resourceId == resourceId; });
}

bool NodeGraph::removeGuideAssignment(
        const String& nodeId,
        const TrimeshCubeComponentGuideTarget& target) {
    const size_t previousCount = guideAssignments.size();
    eraseIf(guideAssignments, [&](const GuideCurveAssignment& assignment) {
        return assignment.targets(nodeId, target);
    });
    if (guideAssignments.size() == previousCount) {
        return false;
    }

    rebuildGuideAssignmentIndexes();
    ++revision;
    return true;
}

const GuideCurveAssignment* NodeGraph::guideAssignmentForTarget(
        const String& nodeId,
        const TrimeshCubeComponentGuideTarget& target) const {
    const auto found = guideAssignmentTargetIndex.find({ nodeId, target });
    if (found != guideAssignmentTargetIndex.end()
            && found->second < guideAssignments.size()) {
        return &guideAssignments[found->second];
    }
    return overlayBase != nullptr
            ? overlayBase->guideAssignmentForTarget(nodeId, target)
            : nullptr;
}

int NodeGraph::removeGuideAssignmentsOutsideCubeRange(
        const String& nodeId,
        int cubeCount,
        std::vector<GuideCurveAssignment>* removed) {
    InteractionComplexityDiagnostics::recordAssignmentLinearScan();
    const size_t previousCount = guideAssignments.size();
    eraseIf(guideAssignments, [&](const GuideCurveAssignment& assignment) {
        const bool shouldRemove = assignment.targetNodeId == nodeId
                && !isPositiveAndBelow(assignment.target.cubeIndex, cubeCount);
        if (shouldRemove && removed != nullptr) {
            removed->push_back(assignment);
        }
        return shouldRemove;
    });
    const int removedCount = (int) (previousCount - guideAssignments.size());
    if (removedCount == 0) {
        return 0;
    }

    rebuildGuideAssignmentIndexes();
    ++revision;
    return removedCount;
}

int NodeGraph::guideUsageCount(const String& guideId) const {
    const auto found = guideUsageCounts.find(guideId);
    return found != guideUsageCounts.end()
            ? found->second
            : (overlayBase != nullptr ? overlayBase->guideUsageCount(guideId) : 0);
}

const std::vector<String>& NodeGraph::guideTargetNodeIds(const String& guideId) const {
    static const std::vector<String> empty;
    const auto found = guideTargetNodes.find(guideId);
    return found != guideTargetNodes.end()
            ? found->second
            : (overlayBase != nullptr ? overlayBase->guideTargetNodeIds(guideId) : empty);
}

const std::vector<String>& NodeGraph::guideIdsForTargetNode(const String& nodeId) const {
    static const std::vector<String> empty;
    const auto found = targetNodeGuides.find(nodeId);
    return found != targetNodeGuides.end()
            ? found->second
            : (overlayBase != nullptr ? overlayBase->guideIdsForTargetNode(nodeId) : empty);
}

void NodeGraph::rebuildGuideResourceIndex() {
    guideResourceIndex.clear();
    guideResourceIndex.reserve(guideCurves.size());
    for (size_t index = 0; index < guideCurves.size(); ++index) {
        guideResourceIndex[guideCurves[index].id] = index;
    }
}

void NodeGraph::rebuildGuideAssignmentIndexes() {
    InteractionComplexityDiagnostics::recordAssignmentLinearScan();
    guideAssignmentTargetIndex.clear();
    guideUsageCounts.clear();
    guideTargetNodes.clear();
    targetNodeGuides.clear();
    guideAssignmentTargetIndex.reserve(guideAssignments.size());
    using StringSet = std::unordered_set<String, StringHash>;
    std::unordered_map<String, StringSet, StringHash> targetNodesByGuide;
    std::unordered_map<String, StringSet, StringHash> guidesByTargetNode;
    for (size_t index = 0; index < guideAssignments.size(); ++index) {
        const GuideCurveAssignment& assignment = guideAssignments[index];
        guideAssignmentTargetIndex[{
                assignment.targetNodeId,
                assignment.target
        }] = index;
        ++guideUsageCounts[assignment.guideId];
        if (targetNodesByGuide[assignment.guideId]
                .insert(assignment.targetNodeId).second) {
            guideTargetNodes[assignment.guideId].push_back(assignment.targetNodeId);
        }
        if (guidesByTargetNode[assignment.targetNodeId]
                .insert(assignment.guideId).second) {
            targetNodeGuides[assignment.targetNodeId].push_back(assignment.guideId);
        }
    }
}

void NodeGraph::addSignalProbe(SignalProbe probe) {
    if (findSignalProbe(probe.id) != nullptr
            || findSignalProbeForSource(probe.sourceNodeId, probe.sourcePortId) != nullptr) {
        return;
    }

    signalProbes.push_back(std::move(probe));
    ++revision;
}

bool NodeGraph::removeSignalProbe(const String& probeId) {
    const size_t previousCount = signalProbes.size();
    eraseIf(signalProbes, [&](const SignalProbe& probe) {
        return probe.id == probeId;
    });
    if (signalProbes.size() == previousCount) {
        return false;
    }

    ++revision;
    return true;
}

const SignalProbe* NodeGraph::findSignalProbe(const String& probeId) const {
    for (const auto& probe : signalProbes) {
        if (probe.id == probeId) {
            return &probe;
        }
    }
    return overlayBase != nullptr ? overlayBase->findSignalProbe(probeId) : nullptr;
}

SignalProbe* NodeGraph::findSignalProbeForEditing(const String& probeId) {
    for (auto& probe : signalProbes) {
        if (probe.id == probeId) {
            return &probe;
        }
    }
    return nullptr;
}

const SignalProbe* NodeGraph::findSignalProbeForSource(
        const String& sourceNodeId,
        const String& sourcePortId) const {
    for (const auto& probe : signalProbes) {
        if (probe.sourceNodeId == sourceNodeId && probe.sourcePortId == sourcePortId) {
            return &probe;
        }
    }
    return overlayBase != nullptr
            ? overlayBase->findSignalProbeForSource(sourceNodeId, sourcePortId)
            : nullptr;
}

void NodeGraph::removeNode(const String& nodeId) {
    const size_t previousNodeCount = nodes.size();
    const size_t previousAssignmentCount = guideAssignments.size();
    eraseIf(nodes, [&](const Node& node) {
        return node.id == nodeId;
    });

    eraseIf(edges, [&](const Edge& edge) {
        return edge.sourceNodeId == nodeId || edge.destNodeId == nodeId;
    });
    eraseIf(guideAssignments, [&](const GuideCurveAssignment& assignment) {
        return assignment.targetNodeId == nodeId;
    });
    if (guideAssignments.size() != previousAssignmentCount) {
        rebuildGuideAssignmentIndexes();
    }
    for (auto& probe : signalProbes) {
        if (probe.sourceNodeId == nodeId) {
            probe.sourceNodeId = {};
            probe.sourcePortId = {};
        }
        if (probe.anchorDestNodeId == nodeId) {
            probe.anchorDestNodeId = {};
            probe.anchorDestPortId = {};
        }
    }
    if (nodes.size() != previousNodeCount) {
        rebuildNodeIndex();
        ++revision;
    }
}

void NodeGraph::removeEdgeAt(size_t index) {
    if (index >= edges.size()) {
        return;
    }

    edges.erase(edges.begin() + (int) index);
    ++revision;
}

bool NodeGraph::removeEdge(const Edge& edgeToRemove) {
    const auto found = std::find_if(edges.begin(), edges.end(), [&](const Edge& edge) {
        return edge.sourceNodeId == edgeToRemove.sourceNodeId
            && edge.sourcePortId == edgeToRemove.sourcePortId
            && edge.destNodeId == edgeToRemove.destNodeId
            && edge.destPortId == edgeToRemove.destPortId
            && edge.connectionKind == edgeToRemove.connectionKind
            && edge.attachmentType == edgeToRemove.attachmentType;
    });
    if (found == edges.end()) {
        return false;
    }
    edges.erase(found);
    ++revision;
    return true;
}

void NodeGraph::removeEdgesToInput(const String& nodeId, const String& portId) {
    const size_t previousEdgeCount = edges.size();
    eraseIf(edges, [&](const Edge& edge) {
        return edge.destNodeId == nodeId && edge.destPortId == portId;
    });
    if (edges.size() != previousEdgeCount) {
        ++revision;
    }
}

void NodeGraph::removeEdgesFromOutput(const String& nodeId, const String& portId) {
    const size_t previousEdgeCount = edges.size();
    eraseIf(edges, [&](const Edge& edge) {
        return edge.sourceNodeId == nodeId && edge.sourcePortId == portId;
    });
    if (edges.size() != previousEdgeCount) {
        ++revision;
    }
}

Colour colourForDomain(PortDomain domain) {
    const Colour control { 0xffc5cad3 };
    switch (domain) {
        case PortDomain::DomainContext:           return control;
        case PortDomain::TimeSignal:              return Colour(0xff35d6d2);
        case PortDomain::SpectralMagnitudeSignal: return Colour(0xffffb347);
        case PortDomain::SpectralPhaseSignal:     return Colour(0xffb284ff);
        case PortDomain::MeshField:               return control;
        case PortDomain::EnvelopeSignal:          return control;
        case PortDomain::PitchSignal:             return control;
        case PortDomain::VoiceControlSignal:      return control;
        case PortDomain::ControlSignal:           return control;
        default:                                  return control;
    }
}

Colour colourForMorphDimension(MorphDimension dimension) {
    switch (dimension) {
        case MorphDimension::Yellow: return Colour(0xffd7bf5f);
        case MorphDimension::Red:    return Colour(0xffd65a5a);
        case MorphDimension::Blue:   return Colour(0xff5f91e8);
    }

    return colourForDomain(PortDomain::ControlSignal);
}

String labelForDomain(PortDomain domain) {
    switch (domain) {
        case PortDomain::DomainContext:           return "Context";
        case PortDomain::TimeSignal:              return "Time";
        case PortDomain::SpectralMagnitudeSignal: return "Mag";
        case PortDomain::SpectralPhaseSignal:     return "Phase";
        case PortDomain::MeshField:               return "Mesh";
        case PortDomain::EnvelopeSignal:          return "Env";
        case PortDomain::PitchSignal:             return "Pitch";
        case PortDomain::VoiceControlSignal:      return "Voice";
        case PortDomain::ControlSignal:           return "Universal";
        default:                                  return "Unknown";
    }
}

String labelForChannelLayout(ChannelLayout layout) {
    switch (layout) {
        case ChannelLayout::Mono:         return "";
        case ChannelLayout::LinkedStereo: return "L/R";
        case ChannelLayout::Left:         return "L";
        case ChannelLayout::Right:        return "R";
        case ChannelLayout::StereoPair:   return "Pair";
        default:                          return "?";
    }
}

String labelForNodeKind(NodeKind kind) {
    const auto* definition = NodeDefinitionRegistry::instance().find(kind);
    return definition != nullptr ? definition->displayName : "Unknown";
}

String idForConnectionKind(ConnectionKind kind) {
    switch (kind) {
        case ConnectionKind::Signal:                  return "signal";
        case ConnectionKind::ConfigurationAttachment: return "configurationAttachment";
        case ConnectionKind::ProcessingAttachment:    return "processingAttachment";
    }

    return {};
}

String idForAttachmentType(AttachmentType type) {
    switch (type) {
        case AttachmentType::None:             return "none";
        case AttachmentType::ScratchEnvelope:  return "scratchEnvelope";
        case AttachmentType::ModulationTriple: return "modulationTriple";
        case AttachmentType::Unison:           return "unison";
    }

    return {};
}

std::optional<ConnectionKind> connectionKindForId(const String& id) {
    if (id == "signal") {
        return ConnectionKind::Signal;
    }
    if (id == "configurationAttachment") {
        return ConnectionKind::ConfigurationAttachment;
    }
    if (id == "processingAttachment") {
        return ConnectionKind::ProcessingAttachment;
    }
    return std::nullopt;
}

std::optional<AttachmentType> attachmentTypeForId(const String& id) {
    if (id == "none") {
        return AttachmentType::None;
    }
    if (id == "scratchEnvelope") {
        return AttachmentType::ScratchEnvelope;
    }
    if (id == "modulationTriple") {
        return AttachmentType::ModulationTriple;
    }
    if (id == "unison") {
        return AttachmentType::Unison;
    }
    return std::nullopt;
}

String parameterValueForNode(const Node& node, const String& parameterId, const String& fallback) {
    return NodeParameterMap(node).stringValue(parameterId, fallback);
}

NodeNaturalSize naturalSizeForNode(const Node& node) {
    const int portRows = jmax((int) node.inputs.size(), (int) node.outputs.size());
    const auto* definition = NodeDefinitionRegistry::instance().find(node.kind);
    const auto preview = definition != nullptr
            ? definition->minimumPreviewSize
            : NodeNaturalSize { 190.f, 76.f };
    if (definition != nullptr && definition->fixedNaturalSize.width > 0.f) {
        return definition->fixedNaturalSize;
    }

    const float titleWidth = (float) labelForNodeKind(node.kind).length() * 8.5f;
    const float subtitleWidth = (float) node.subtitle.length() * 6.0f;

    const float headerWidth = titleWidth + subtitleWidth + 72.f;
    const float portWidth = 120.f;
    const float previewWidth = preview.width + 26.f;
    const float width = jmax(headerWidth, portWidth, previewWidth);

    const float headerHeight = 42.f;
    const float portHeight = 16.f + (float) portRows * 34.f;
    const float previewHeight = preview.height + 26.f;
    const float height = headerHeight + portHeight + previewHeight;

    return { width, height };
}

}
