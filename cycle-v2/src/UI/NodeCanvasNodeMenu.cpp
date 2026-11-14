#include "UI/NodeCanvas.h"

namespace CycleV2 {

void NodeCanvas::showNodeCreationMenu(Point<float> canvasPosition) {
    PopupMenu menu;
    std::vector<NodeKind> kinds;
    for (int index = 0; index < palette.sectionCount(); ++index) {
        const auto& section = palette.section(index);
        PopupMenu entries;
        for (int entry = 0; entry < section.entryCount; ++entry) {
            kinds.push_back(section.entries[entry].kind);
            entries.addItem((int) kinds.size(), section.entries[entry].label);
        }
        menu.addSubMenu(section.title, entries);
    }
    const Point<float> worldPosition = viewport.toWorld(canvasPosition);
    const auto screenPosition = localPointToGlobal(canvasPosition.roundToInt());
    const Component::SafePointer<NodeCanvas> safe(this);
    const auto options = PopupMenu::Options().withTargetComponent(this)
            .withTargetScreenArea({ screenPosition.x, screenPosition.y, 1, 1 });
    lastPopupMenuTargetScreenArea = options.getTargetScreenArea();
    menu.showMenuAsync(options,
            [safe, worldPosition, kinds = std::move(kinds)](int result) {
                if (safe != nullptr && result > 0 && result <= (int) kinds.size()) {
                    safe->applyAuthoringResult(safe->authoring.addNode(
                            kinds[(size_t) result - 1], worldPosition));
                    safe->requestCanvasRepaint();
                }
            });
}

}
