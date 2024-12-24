#include "JuceHeader.h"
#include <Definitions.h>
#include <App/SingletonRepo.h>
#include <UI/IConsole.h>

#include "WaveDragTarget.h"

WaveDragTarget::WaveDragTarget(SingletonRepo* repo) : SingletonAccessor(repo, "WaveDragTarget") {
}

bool WaveDragTarget::isInterestedInFileDrag(const StringArray &files) {
    for (const auto& file : files) {
        if (isFileGood(file)) {
            return true;
        }
    }

    return false;
}

void WaveDragTarget::fileDragEnter(const StringArray &files, int x, int y) {
    if (isInterestedInFileDrag(files)) {
        showConsoleMsg("Drop audio file to analyse");
    }
}

void WaveDragTarget::fileDragExit(const StringArray &files) {
}

void WaveDragTarget::filesDropped(const StringArray &files, int x, int y) {
    if (listener == nullptr)
        return;

    for (const auto& i : files) {
        File file(i);
        if (isFileGood(file)) {
            listener->loadWave(file);
            return;
        }
    }
}

bool WaveDragTarget::isFileGood(const File &file) {
    if (file.existsAsFile() && String(".wav.aiff.ogg.flac").containsIgnoreCase(file.getFileExtension())) {
        return true;
    }

    return false;
}
