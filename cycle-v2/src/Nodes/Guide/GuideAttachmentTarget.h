#pragma once

namespace CycleV2 {

struct Node;
struct TrimeshCubeComponentGuideTarget;

class GuideAttachmentTarget {
public:
    static constexpr int fieldCount = 6;
    static bool isValid(const Node& node, const TrimeshCubeComponentGuideTarget& target);
};

}
