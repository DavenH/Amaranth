#include "App/GraphFileHistory.h"

namespace CycleV2 {

GraphFileHistory::GraphFileHistory(PropertiesFile& properties) :
        properties(properties) {
    recentFiles.setMaxNumberOfItems(maximumRecentFiles);
    recentFiles.restoreFromString(properties.getValue(recentFilesKey));
    recentFiles.removeNonExistentFiles();
    lastOpenDirectory = File(properties.getValue(lastOpenDirectoryKey));
}

File GraphFileHistory::initialOpenDirectory(const File& fallback) const {
    return lastOpenDirectory.isDirectory() ? lastOpenDirectory : fallback;
}

void GraphFileHistory::recordOpened(const File& file) {
    if (!file.existsAsFile()) {
        return;
    }

    lastOpenDirectory = file.getParentDirectory();
    recentFiles.addFile(file);
    persist();
}

void GraphFileHistory::recordSaved(const File& file) {
    if (!file.existsAsFile()) {
        return;
    }

    recentFiles.addFile(file);
    persist();
}

int GraphFileHistory::addRecentMenuItems(PopupMenu& menu) {
    recentFiles.removeNonExistentFiles();
    return recentFiles.createPopupMenuItems(
            menu, recentMenuBaseId, true, true);
}

bool GraphFileHistory::isRecentMenuItem(int menuItemId) const {
    const int index = menuItemId - recentMenuBaseId;
    return isPositiveAndBelow(index, recentFiles.getNumFiles());
}

File GraphFileHistory::fileForMenuItem(int menuItemId) const {
    if (!isRecentMenuItem(menuItemId)) {
        return {};
    }

    return recentFiles.getFile(menuItemId - recentMenuBaseId);
}

void GraphFileHistory::persist() {
    properties.setValue(recentFilesKey, recentFiles.toString());
    properties.setValue(lastOpenDirectoryKey, lastOpenDirectory.getFullPathName());
    properties.saveIfNeeded();
}

}
