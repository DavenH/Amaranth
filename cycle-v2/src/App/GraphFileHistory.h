#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

using namespace juce;

class GraphFileHistory {
public:
    static constexpr int recentMenuBaseId = 0x3100;
    static constexpr int maximumRecentFiles = 10;

    explicit GraphFileHistory(PropertiesFile& properties);

    File initialOpenDirectory(const File& fallback) const;
    void recordOpened(const File& file);
    void recordSaved(const File& file);

    int addRecentMenuItems(PopupMenu& menu);
    bool isRecentMenuItem(int menuItemId) const;
    File fileForMenuItem(int menuItemId) const;

    int getNumRecentFiles() const { return recentFiles.getNumFiles(); }
    File getRecentFile(int index) const { return recentFiles.getFile(index); }

private:
    static constexpr const char* recentFilesKey = "recentGraphFiles";
    static constexpr const char* lastOpenDirectoryKey = "lastOpenGraphDirectory";

    PropertiesFile& properties;
    RecentlyOpenedFilesList recentFiles;
    File lastOpenDirectory;

    void persist();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphFileHistory)
};

}
