#pragma once

#include "../Graph/NodeDefinition.h"

namespace CycleV2 {

struct NodeModuleDescriptor {
    NodeKind kind { NodeKind::GenericProcessor };
    AudioModuleRole audioRole { AudioModuleRole::None };
    PreviewModuleRole previewRole { PreviewModuleRole::None };
    bool executable {};
    bool previewable {};
    bool cycle1AdapterBacked {};
    String cycle1Reference;
};

class NodeModuleRegistry {
public:
    NodeModuleDescriptor descriptorFor(NodeKind kind) const;
};

String labelForAudioModuleRole(AudioModuleRole role);
String labelForPreviewModuleRole(PreviewModuleRole role);

}
