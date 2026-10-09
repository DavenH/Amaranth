#pragma once

#include <JuceHeader.h>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class NodePalette {
public:
    struct Entry {
        NodeKind kind;
        const char* label;
    };

    struct Section {
        const char* title;
        const Entry* entries {};
        int entryCount {};
    };

    void setWorkspaceBounds(Rectangle<float> bounds);
    void setVisible(bool value);
    bool isVisible() const { return visible; }
    Rectangle<float> workspaceBounds() const { return workspace; }
    int sectionCount() const;
    const Section& section(int sectionIndex) const;

    std::vector<std::pair<String, Rectangle<float>>> pointerTargets() const;
    Rectangle<float> railBounds() const;
    Rectangle<float> groupBounds(int sectionIndex) const;
    Rectangle<float> entryBounds(int sectionIndex, int entryIndex) const;

    int activeSection() const { return activeSectionIndex; }
    int activeEntry() const { return activeEntryIndex; }
    int findSectionAt(Point<float> screenPosition) const;
    bool findKindAt(Point<float> screenPosition, NodeKind& kind) const;
    bool updateHover(Point<float> screenPosition);
    bool close();

private:
    float tileWidth() const;
    float tileHeight() const;
    float groupHeight(int sectionIndex) const;

    Rectangle<float> workspace { 0.f, 0.f, 290.f, 962.f };
    bool visible { true };
    int activeSectionIndex { -1 };
    int activeEntryIndex { -1 };
};

}
