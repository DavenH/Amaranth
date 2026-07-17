#pragma once

#include <JuceHeader.h>

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

class NodePalette {
public:
    struct Entry {
        NodeKind kind;
        const char* label;
    };

    struct Section {
        const char* title;
        const char* shortLabel;
        PortDomain domain {};
        const Entry* entries {};
        int entryCount {};
    };

    int sectionCount() const;
    const Section& section(int sectionIndex) const;

    Rectangle<float> railBounds() const;
    Rectangle<float> groupBounds(int sectionIndex) const;
    Rectangle<float> pulloutBounds(int sectionIndex) const;
    Rectangle<float> entryBounds(int sectionIndex, int entryIndex) const;
    Rectangle<float> hoverBounds(int sectionIndex) const;

    int activeSection() const { return activeSectionIndex; }
    int findSectionAt(Point<float> screenPosition) const;
    bool findKindAt(Point<float> screenPosition, NodeKind& kind) const;
    bool updateHover(Point<float> screenPosition);
    bool close();

private:
    int groupIndexAt(Point<float> screenPosition) const;

    int activeSectionIndex { -1 };
};

}
