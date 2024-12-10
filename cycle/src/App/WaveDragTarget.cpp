#include <Definitions.h>
#include <App/SingletonRepo.h>
#include <UI/IConsole.h>

#include "WaveDragTarget.h"

WaveDragTarget::WaveDragTarget(SingletonRepo* repo) : SingletonAccessor(repo, "WaveDragTarget") {

}

bool WaveDragTarget::isInterestedInFileDrag(const StringArray &files) {
#ifndef BEAT_EDITION
    for (int i = 0; i < files.size(); ++i) {
        if (isFileGood(files[i]))
            return true;
    }
#endif

    return false;
}


void WaveDragTarget::fileDragEnter(const StringArray &files, int x, int y) {
#ifndef BEAT_EDITION
    if (isInterestedInFileDrag(files)) {
        showMsg("Drop audio file to analyse");
    }
#endif
}


void WaveDragTarget::fileDragExit(const StringArray &files) {

}


void WaveDragTarget::filesDropped(const StringArray &files, int x, int y) {
    if (listener == nullptr)
        return;

#ifndef BEAT_EDITION
    for (int i = 0; i < files.size(); ++i) {
        File file(files[i]);
        if (isFileGood(file)) {
            listener->loadWave(file);
            return;
        }
    }
#endif
}


bool WaveDragTarget::isFileGood(const File &file) {
#ifndef BEAT_EDITION
    if (file.existsAsFile() && String(".wav.aiff.ogg.flac").containsIgnoreCase(file.getFileExtension())) {
        return true;
    }
#endif

    return false;
}
